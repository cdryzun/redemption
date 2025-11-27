/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2017
   Author(s): Christophe Grosjean, Jonatan Poelen
*/

#include "capture/capture_params.hpp"
#include "capture/video_params.hpp"
#include "capture/full_video_params.hpp"
#include "capture/sequenced_video_params.hpp"
#include "capture/video_capture.hpp"
#include "capture/video_recorder.hpp"
#include "utils/drawable.hpp"
#include "utils/sugar/byte_copy.hpp"

#include "core/RDP/RDPDrawable.hpp"

#include "gdi/capture_api.hpp"

#include "utils/log.hpp"
#include "utils/strutils.hpp"

#include <cerrno>
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <ctime>


using namespace std::chrono_literals;

using ImageByInterval = VideoCaptureCtx::ImageByInterval;

namespace
{
    inline tm to_tm_t(
        MonotonicTimePoint t,
        MonotonicTimeToRealTime monotonic_to_real)
    {
        tm res;
        auto duration = monotonic_to_real.to_real_time_duration(t);
        time_t sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        localtime_r(&sec, &res);
        return res;
    }

    inline ImageByInterval video_params_to_image_by_interval(bool no_timestamp)
    {
        return no_timestamp
            ? ImageByInterval::ZeroOrOneWithoutTimestamp
            : ImageByInterval::ZeroOrOneWithTimestamp
            ;
    }
} // anonymous namespace


using WaitingTimeBeforeNextSnapshot = gdi::CaptureApi::WaitingTimeBeforeNextSnapshot;

// VideoCaptureCtx
//@{
VideoCaptureCtx::VideoCropper::VideoCropper(Drawable& drawable, Rect crop_rect)
: crop_rect(crop_rect)
, original_dimension(crop_rect.cx, crop_rect.cy)
, is_fullscreen(drawable.width() == crop_rect.width()
             && drawable.height() == crop_rect.height())
, out_bmpdata(this->is_fullscreen
    ? nullptr
    : new uint8_t[drawable.width() * drawable.height() * drawable.Bpp] {} /*NOLINT*/
)
{
}

void VideoCaptureCtx::VideoCropper::set_cropping(Rect cropping) noexcept
{
    if (this->is_fullscreen) {
        return ;
    }

    assert(cropping.cx <= original_dimension.w);
    assert(cropping.cy <= original_dimension.h);

    if (cropping.cx != crop_rect.cx) {
        uint8_t* out_bmpdata_tmp = out_bmpdata.get() + cropping.cx * Drawable::Bpp;
        std::size_t const rowsize = original_dimension.w * Drawable::Bpp;
        std::size_t const empty_rowsize = (original_dimension.w - cropping.cx) * Drawable::Bpp;

        for (uint16_t i = 0; i < cropping.cy; ++i) {
            std::memset(out_bmpdata_tmp, 0, empty_rowsize);
            out_bmpdata_tmp += rowsize;
        }
    }

    if (cropping.cx != crop_rect.cx || cropping.cy != crop_rect.cy) {
        uint8_t* out_bmpdata_tmp = out_bmpdata.get() + original_dimension.w * cropping.cy * Drawable::Bpp;
        std::size_t const rowsize = original_dimension.w * Drawable::Bpp;

        for (uint16_t i = cropping.cy; i < original_dimension.h; ++i) {
            std::memset(out_bmpdata_tmp, 0, rowsize);
            out_bmpdata_tmp += rowsize;
        }
    }

    crop_rect = cropping;
}

void VideoCaptureCtx::VideoCropper::prepare_image_frame(Drawable& drawable) noexcept
{
    if (this->is_fullscreen) {
        return ;
    }

    uint8_t* out_bmpdata_tmp = this->out_bmpdata.get();

    uint8_t const* in_bmpdata_tmp
        = drawable.data()
        + checked_cast<size_t>(this->crop_rect.y) * drawable.rowsize()
        + checked_cast<size_t>(this->crop_rect.x) * drawable.Bpp;

    unsigned const rowsize = this->original_dimension.w * drawable.Bpp;
    unsigned const datasize = this->crop_rect.cx * drawable.Bpp;

    for (uint16_t i = 0; i < this->crop_rect.cy; ++i) {
        std::memcpy(out_bmpdata_tmp, in_bmpdata_tmp, datasize);

        in_bmpdata_tmp  += drawable.rowsize();
        out_bmpdata_tmp += rowsize;
    }
}

WritableImageView VideoCaptureCtx::VideoCropper::get_image(Drawable& drawable) noexcept
{
    if (this->is_fullscreen) {
        return gdi::get_writable_image_view(drawable);
    }

    return WritableImageView{
        this->out_bmpdata.get(),
        this->original_dimension.w,
        this->original_dimension.h,
        this->original_dimension.w * drawable.Bpp,
        BytesPerPixel(drawable.Bpp),
        WritableImageView::Storage::TopToBottom,
    };
}

VideoCaptureCtx::VideoCaptureCtx(
    CaptureParams const & capture_params,
    Drawable & drawable,
    LazyDrawablePointer & lazy_drawable_pointer,
    Rect crop_rect,
    VideoParams const & video_params
)
: drawable(drawable)
, lazy_drawable_pointer(lazy_drawable_pointer)
, monotonic_last_time_capture(capture_params.now)
, monotonic_to_real(capture_params.now, capture_params.real_now)
// `1000000L % frame_rate` should be equal to 0
, frame_interval(std::chrono::microseconds(1000000L / video_params.frame_rate))
, next_trace_time(capture_params.now)
, image_by_interval(video_params_to_image_by_interval(video_params.no_timestamp))
, has_timestamp(image_by_interval == ImageByInterval::ZeroOrOneWithTimestamp)
, video_cropper(drawable, crop_rect)
, rect_tracker(drawable.width(), drawable.height())
, updatable_frame_marker_end_bitset_stream(video_params.updatable_frame_marker_end_bitset_view.data())
, updatable_frame_marker_end_bitset_end(video_params.updatable_frame_marker_end_bitset_view.end())
, image_capture {
    .enabled = video_params.thumbnail.enabled,
    .filename_generator = video_params.thumbnail.enabled
        ? FilenameGenerator{ capture_params.record_path, capture_params.basename, "png" }
        : FilenameGenerator::NoNameWithOneCounter{},
    .scaled_png {
        video_params.thumbnail.width,
        video_params.thumbnail.height,
        video_params.thumbnail.use_proportional_geometry,
    },
}
{
    if (video_params.verbosity) {
        LOG(LOG_INFO, "Video recording: codec: %s, frame_rate: %u, options: %s",
            video_params.codec, video_params.frame_rate, video_params.codec_options);
    }
}

void VideoCaptureCtx::preparing_video_frame(video_recorder & recorder)
{
    DrawablePointer::BufferSaver buffer_saver;

    auto& drawable_pointer = this->lazy_drawable_pointer.drawable_pointer();

    drawable_pointer.trace_mouse(this->drawable, buffer_saver);

    auto image = this->prepare_image_frame();

    if (this->has_timestamp) {
        this->timestamp_tracer.trace(image, this->get_tm());
    }
    // Cropping not working, why ?
    if(video_cropper.is_cropped()) {
        recorder.preparing_video_frame();
    } else {
        recorder.preparing_video_frame(rect_tracker.get_rect().disjunct(drawable_pointer.get_rect(drawable)).ebottom());
    }

    if (this->has_timestamp) {
        this->timestamp_tracer.clear(image);
    }

    drawable_pointer.clear_mouse(this->drawable, buffer_saver);
}

WritableImageView VideoCaptureCtx::prepare_image_frame() noexcept
{
    // TODO could be avoided when updatable_graphics.has_drawing_event() == false,
    // but the mouse pointer is drawn on Drawable
    this->video_cropper.prepare_image_frame(this->drawable);
    return this->video_cropper.get_image(this->drawable);
}

void VideoCaptureCtx::frame_marker_event(
    video_recorder & recorder, MonotonicTimePoint now,
    uint16_t cursor_x, uint16_t cursor_y)
{
    if (((updatable_frame_marker_end_bitset_stream.current() == updatable_frame_marker_end_bitset_end
       || updatable_frame_marker_end_bitset_stream.read()
      ) && this->rect_tracker.has_drawing_event())
     || this->cursor_x != cursor_x
     || this->cursor_y != cursor_y
    ) {
        this->preparing_video_frame(recorder);
        this->rect_tracker.reset();
        this->cursor_x = cursor_x;
        this->cursor_y = cursor_y;
    }

    this->has_frame_marker = true;

    this->snapshot(recorder, now, cursor_x, cursor_y);
}

void VideoCaptureCtx::encoding_end_frame(video_recorder & recorder)
{
    auto dur = std::max(this->frame_interval, MonotonicTimePoint::duration(400ms));
    auto save_monotonic_last_time_capture = this->monotonic_last_time_capture;
    auto save_next_trace_time = this->next_trace_time;
    this->snapshot(
        recorder, this->monotonic_last_time_capture + dur,
        this->cursor_x, this->cursor_y);
    this->monotonic_last_time_capture = save_monotonic_last_time_capture;
    this->next_trace_time = save_next_trace_time;
}

void VideoCaptureCtx::next_video(video_recorder & recorder)
{
    this->frame_index = 0;
    this->preparing_video_frame(recorder);
    recorder.encoding_video_frame(++this->frame_index);
    this->rect_tracker.set_area(this->drawable.width(), this->drawable.height());
}

void VideoCaptureCtx::synchronize_times(MonotonicTimePoint monotonic_time, RealTimePoint real_time)
{
    this->monotonic_to_real = MonotonicTimeToRealTime(monotonic_time, real_time);
}

void VideoCaptureCtx::set_cropping(Rect cropping) noexcept
{
    assert(cropping.x >= 0);
    assert(cropping.y >= 0);
    assert(cropping.eright() <= this->drawable.width());
    assert(cropping.ebottom() <= this->drawable.height());

    this->video_cropper.set_cropping(cropping);
    this->rect_tracker.set_area(this->drawable.width(), this->drawable.height());
}

bool VideoCaptureCtx::logical_frame_ended() const noexcept
{
    return this->drawable.logical_frame_ended;
}

WritableImageView VideoCaptureCtx::acquire_image_for_dump(
    DrawablePointer::BufferSaver& buffer_saver,
    const tm& now)
{
    auto& drawable_pointer = this->lazy_drawable_pointer.drawable_pointer();
    drawable_pointer.trace_mouse(this->drawable, buffer_saver);

    auto image = this->prepare_image_frame();

    if (this->has_timestamp) {
        this->timestamp_tracer.trace(image, now);
    }

    return image;
}

void VideoCaptureCtx::release_image_for_dump(
    WritableImageView image,
    DrawablePointer::BufferSaver const& buffer_saver)
{
    if (this->has_timestamp) {
        this->timestamp_tracer.clear(image);
    }

    auto& drawable_pointer = this->lazy_drawable_pointer.drawable_pointer();
    drawable_pointer.clear_mouse(this->drawable, buffer_saver);
}

tm VideoCaptureCtx::get_tm() const
{
    return to_tm_t(this->monotonic_last_time_capture, this->monotonic_to_real);
}

void VideoCaptureCtx::update_fullscreen()
{
    this->rect_tracker.set_area(drawable.width(), drawable.height());
}

WaitingTimeBeforeNextSnapshot VideoCaptureCtx::snapshot(
    video_recorder & recorder, MonotonicTimePoint now,
    uint16_t cursor_x, uint16_t cursor_y
)
{
    auto tick = now - this->monotonic_last_time_capture;
    auto const frame_interval = this->frame_interval;
    if (tick >= frame_interval) {
        bool const update_timestamp = this->has_timestamp
                                   && now >= this->next_trace_time;
        bool const update_image = (!this->has_frame_marker
                                  && this->rect_tracker.has_drawing_event())
                                || this->cursor_x != cursor_x
                                || this->cursor_y != cursor_y
                                ;
        bool const update_pointer = (update_image || update_timestamp);

        DrawablePointer::BufferSaver buffer_saver;

        bool const is_image_drawable
            = update_pointer
            || (update_image
                && (!this->has_timestamp || this->monotonic_last_time_capture < this->next_trace_time));
        Rect rect = is_image_drawable ? rect_tracker.get_rect() : Rect();

        if (update_pointer) {
            auto& drawable_pointer = this->lazy_drawable_pointer.drawable_pointer();
            drawable_pointer.trace_mouse(this->drawable, buffer_saver);
            rect = rect.disjunct(drawable_pointer.get_rect(drawable));
        }

        auto image = WritableImageView::create_null_view();

        if (is_image_drawable) {
            image = this->prepare_image_frame();

            if (this->has_timestamp) {
                this->timestamp_tracer.trace(image, this->get_tm());

                if (this->monotonic_last_time_capture >= this->next_trace_time) {
                    this->next_trace_time += 1s;
                }
            }
            // Cropping not working, why ?
            if(video_cropper.is_cropped()) {
                recorder.preparing_video_frame();
            } else {
                recorder.preparing_video_frame(rect.ebottom());
            }
        }

        this->cursor_x = cursor_x;
        this->cursor_y = cursor_y;

        // synchronize video time with the end of second

        auto preparing_timestamp_video_frame = [&, this](video_recorder & recorder){
            if (not image.data()) {
                image = this->prepare_image_frame();
            }

            this->timestamp_tracer.trace(image, this->get_tm());

            recorder.preparing_timestamp_video_frame();
        };

        switch (this->image_by_interval) {
            case ImageByInterval::ZeroOrOneWithTimestamp:
                do {
                    if (this->monotonic_last_time_capture >= this->next_trace_time) {
                        preparing_timestamp_video_frame(recorder);
                        this->next_trace_time += 1s;
                    }

                    auto update_timer_at = std::max(this->monotonic_last_time_capture + frame_interval, this->next_trace_time)
                                         - this->monotonic_last_time_capture;
                    auto elapsed = std::min(tick, update_timer_at);
                    auto count = elapsed / frame_interval;
                    elapsed = count * frame_interval;

                    this->frame_index += count;
                    recorder.encoding_video_frame(this->frame_index);

                    this->monotonic_last_time_capture += elapsed;
                    tick -= elapsed;
                } while (this->monotonic_last_time_capture + frame_interval <= now);
                break;

            case ImageByInterval::ZeroOrOneWithoutTimestamp:
                constexpr MonotonicTimePoint::duration max_frame_interval = 2s;
                do {
                    auto elapsed = std::min(tick, max_frame_interval);
                    auto count = elapsed / frame_interval;
                    elapsed = count * frame_interval;

                    this->frame_index += count;
                    recorder.encoding_video_frame(this->frame_index);

                    this->monotonic_last_time_capture += elapsed;
                    tick -= elapsed;
                } while (this->monotonic_last_time_capture + frame_interval <= now);
                break;
        }

        this->rect_tracker.reset();

        if (update_timestamp && this->has_timestamp) {
            this->timestamp_tracer.clear(image);
        }

        if (update_pointer) {
            auto& drawable_pointer = this->lazy_drawable_pointer.drawable_pointer();
            drawable_pointer.clear_mouse(this->drawable, buffer_saver);
        }
    }
    return WaitingTimeBeforeNextSnapshot(frame_interval - tick);
}

void VideoCaptureCtx::generate_thumbnail(tm const& now)
{
    if (!has_thumbnail_feature()) {
        return ;
    }

    DrawablePointer::BufferSaver buffer_saver;
    auto image = this->acquire_image_for_dump(buffer_saver, now);
    auto & filename = image_capture.filename_generator.current_filename();
    image_capture.scaled_png.dump_png24(filename.c_str(), image, true);
    release_image_for_dump(image, buffer_saver);
    image_capture.filename_generator.next();
}

//@}


// FullVideoCaptureImpl
//@{

FullVideoCaptureImpl::FullVideoCaptureImpl(
    CaptureParams const & capture_params,
    Drawable & drawable,
    LazyDrawablePointer & lazy_drawable_pointer,
    Rect crop_rect,
    VideoParams const & video_params,
    FullVideoParams const & full_video_params)
: start_time(capture_params.now)
, last_time_thumbnail(capture_params.now)
, thumbnail_interval(
    video_params.thumbnail.enabled
    && full_video_params.thumbnail_interval > MonotonicTimePoint::duration::zero()
        ? full_video_params.thumbnail_interval
        : MonotonicTimePoint::duration::zero())
, recorder(
    str_concat(capture_params.record_path, capture_params.basename, '.', video_params.codec).c_str(),
    capture_params.file_permissions,
    capture_params.session_log,
    drawable,
    checked_int{video_params.frame_rate},
    video_params.codec.c_str(),
    video_params.codec_options.c_str(),
    checked_int{video_params.verbosity})
, thumbnail_file{[&]{
    FILE * f = nullptr;

    if (!video_params.thumbnail.enabled) {
        return f;
    }

    auto fname = str_concat(capture_params.record_path, capture_params.basename, ".thumbnails.json");
    f = fopen(fname.c_str(), "w");
    if (!f) {
        int errnum = errno;
        LOG(LOG_ERR, "Open .thumbnails file error: %s [%d]", strerror(errnum), errnum);
        throw Error(ERR_RECORDER_SNAPSHOT_FAILED, errnum);
    }

    return f;
}()}
, video_cap_ctx(
    capture_params, drawable, lazy_drawable_pointer, crop_rect, video_params)
{
}

FullVideoCaptureImpl::~FullVideoCaptureImpl()
{
    if (this->thumbnail_file) {
        if (this->video_cap_ctx.thumbnail_counter()) {
            constexpr auto last_part = "\"}]"_av;
            fwrite(last_part.data(), last_part.size(), 1, this->thumbnail_file);
        }
        else {
            fwrite("[]", 2, 1, this->thumbnail_file);
        }
        fclose(this->thumbnail_file);
    }
    this->video_cap_ctx.encoding_end_frame(this->recorder);
}

WaitingTimeBeforeNextSnapshot FullVideoCaptureImpl::periodic_snapshot(
    MonotonicTimePoint now, uint16_t cursor_x, uint16_t cursor_y)
{
    auto ret = this->video_cap_ctx.snapshot(this->recorder, now, cursor_x, cursor_y);

    if (this->thumbnail_interval > MonotonicTimePoint::duration::zero()
     && this->last_time_thumbnail + this->thumbnail_interval <= now
    ) {
        next_thumbnail(now);
        ret.set_min(this->thumbnail_interval);
    }

    return ret;
}

void FullVideoCaptureImpl::next_thumbnail(MonotonicTimePoint now)
{
    if (!this->thumbnail_file) {
        return ;
    }

    constexpr auto part1_first = "[{\"time\":"_av;
    constexpr auto part1_other = "\"},{\"time\":"_av;
    constexpr auto part2 = ",\"filename\":\""_av;
    char buf[
        buffer_size_of_uint64_to_chars
      + part1_first.size()
      + part1_other.size()
      + part2.size()
    ];

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->start_time);
    auto dur_as_uint = static_cast<uint64_t>(dur.count());

    char * p = buf;
    if (this->video_cap_ctx.thumbnail_counter()) {
        p = unchecked_bytes_copy_and_advance(p, part1_other);
    }
    else {
        p = unchecked_bytes_copy_and_advance(p, part1_first);
    }
    p = unchecked_bytes_copy_and_advance(p, int_to_decimal_chars(dur_as_uint).sv());
    p = unchecked_bytes_copy_and_advance(p, part2);

    fwrite(buf, checked_int{p - buf}, 1, this->thumbnail_file);

    auto const & filename = this->video_cap_ctx.current_thumbnail_filename();
    fwrite(filename.data(), filename.size(), 1, this->thumbnail_file);

    auto monotonic_to_real = this->video_cap_ctx.get_monotonic_to_real();
    this->video_cap_ctx.generate_thumbnail(to_tm_t(now, monotonic_to_real));
    this->last_time_thumbnail = now;
}


VideoCaptureCtx::FilenameGenerator::FilenameGenerator(
    std::string_view prefix,
    std::string_view filename,
    std::string_view extension)
: filename(str_concat(prefix, filename, "-000000."_av, extension))
, num_pos(int(this->filename.size() - (extension.size() + 1)))
{}

VideoCaptureCtx::FilenameGenerator::FilenameGenerator(NoNameWithOneCounter)
: num(1)
{}

void VideoCaptureCtx::FilenameGenerator::next()
{
    ++this->num;
    auto chars = int_to_decimal_chars(this->num);
    memcpy(this->filename.data() + this->num_pos - chars.size(), chars.data(), chars.size());
}

//@}


// SequencedVideoCaptureImpl
//@{

WaitingTimeBeforeNextSnapshot SequencedVideoCaptureImpl::periodic_snapshot(
    MonotonicTimePoint now, uint16_t cursor_x, uint16_t cursor_y)
{
    this->video_cap_ctx.snapshot(*this->recorder, now, cursor_x, cursor_y);
    if (this->video_cap_ctx.thumbnail_counter()) {
        return this->video_sequencer_periodic_snapshot(now);
    }
    else {
        return this->first_periodic_snapshot(now);
    }
}

WaitingTimeBeforeNextSnapshot SequencedVideoCaptureImpl::first_periodic_snapshot(MonotonicTimePoint now)
{
    WaitingTimeBeforeNextSnapshot ret;

    auto constexpr interval = std::chrono::microseconds(3s) / 2;
    auto const duration = now - this->monotonic_start_capture;
    if (duration >= interval) {
        auto video_interval = this->break_interval;
        if (this->video_cap_ctx.logical_frame_ended()
         || duration > 2s
         || duration >= video_interval
        ) {
            auto monotonic_to_real = this->video_cap_ctx.get_monotonic_to_real();
            this->video_cap_ctx.generate_thumbnail(to_tm_t(now, monotonic_to_real));
            ret = WaitingTimeBeforeNextSnapshot(video_interval);
        }
        else {
            ret = WaitingTimeBeforeNextSnapshot(interval / 3);
        }
    }
    else {
        ret = WaitingTimeBeforeNextSnapshot(interval - duration);
    }

    ret.set_min(this->video_sequencer_periodic_snapshot(now).duration());
    return ret;
}


void SequencedVideoCaptureImpl::init_recorder()
{
    DrawablePointer::BufferSaver buffer_saver;
    const auto now = this->video_cap_ctx.get_tm();
    auto image = this->video_cap_ctx.acquire_image_for_dump(buffer_saver, now);
    this->recorder.emplace(
        this->vc_filename_generator.current_filename().c_str(),
        this->recorder_params.file_permissions,
        this->recorder_params.acl_report,
        image,
        this->recorder_params.frame_rate,
        this->recorder_params.codec_name.c_str(),
        this->recorder_params.codec_options.c_str(),
        this->recorder_params.verbosity
    );
    this->video_cap_ctx.release_image_for_dump(image, buffer_saver);
}

WaitingTimeBeforeNextSnapshot SequencedVideoCaptureImpl::video_sequencer_periodic_snapshot(
    MonotonicTimePoint now)
{
    assert(this->break_interval.count());
    auto const interval = now - this->start_break;
    if (interval >= this->break_interval) {
        this->next_video_impl(now, NotifyNextVideo::Reason::sequenced);
        return WaitingTimeBeforeNextSnapshot(this->break_interval);
    }
    return WaitingTimeBeforeNextSnapshot(this->break_interval - interval);
}


SequencedVideoCaptureImpl::SequencedVideoCaptureImpl(
    CaptureParams const & capture_params,
    Drawable & drawable,
    LazyDrawablePointer & lazy_drawable_pointer,
    Rect crop_rect,
    VideoParams const & video_params,
    SequencedVideoParams const& sequenced_video_params,
    NotifyNextVideo & next_video_notifier)
: monotonic_start_capture(capture_params.now)
, vc_filename_generator(capture_params.record_path, capture_params.basename, video_params.codec)
, start_break(capture_params.now)
, break_interval((sequenced_video_params.break_interval > std::chrono::microseconds::zero())
    ? sequenced_video_params.break_interval
    : std::chrono::microseconds::max())
, next_video_notifier(next_video_notifier)
, recorder_params{
    capture_params.session_log,
    video_params.codec,
    video_params.codec_options,
    int(video_params.frame_rate),
    int(video_params.verbosity),
    capture_params.file_permissions,
}
, video_cap_ctx(capture_params, drawable, lazy_drawable_pointer, crop_rect, video_params)
{
    this->init_recorder();
}

SequencedVideoCaptureImpl::~SequencedVideoCaptureImpl()
{
    if (this->recorder) {
        this->video_cap_ctx.encoding_end_frame(*this->recorder);
    }
}

void SequencedVideoCaptureImpl::next_video_impl(MonotonicTimePoint now, NotifyNextVideo::Reason reason)
{
    this->start_break = now;

    tm ptm = to_tm_t(now, this->video_cap_ctx.get_monotonic_to_real());

    if (!this->video_cap_ctx.thumbnail_counter()) {
        this->video_cap_ctx.generate_thumbnail(ptm);
    }

    this->video_cap_ctx.encoding_end_frame(*this->recorder);
    this->recorder.reset();
    this->vc_filename_generator.next();

    this->init_recorder();
    this->video_cap_ctx.next_video(*this->recorder);

    this->video_cap_ctx.generate_thumbnail(ptm);

    this->next_video_notifier.notify_next_video(now, reason);
}

void SequencedVideoCaptureImpl::next_video(MonotonicTimePoint now)
{
    this->next_video_impl(now, NotifyNextVideo::Reason::external);
}

//@}
