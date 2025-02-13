import os
import time
from unittest import TestCase
from unittest.mock import patch, Mock
from collections.abc import Callable
from typing import NamedTuple, Optional, Tuple
from sesmanworker import Sesman


class DecoContext(NamedTuple):
    getdeco: Mock
    interdsiplay: Mock


class TimeTest(NamedTuple):
    timezone: int
    altzone: int
    daylight: int
    localtime: Callable[[], time.struct_time]
    time: Callable[[], float]
    mktime: Callable[[time.struct_time], float]


struct_time_isdst = time.struct_time((0,0,0,0,0,0,0,0, 1))
struct_time_isnotdst = time.struct_time((0,0,0,0,0,0,0,0, 0))


class TestSesmanCheckDeconnectionTime(TestCase):
    @staticmethod
    def run_deco(
            context: DecoContext,
            deco_time: str,
            expect_result: Tuple[Optional[int], bool, str],
            expect_message: Optional[str],
            time_for_tests: Optional[TimeTest] = None,
    ) -> None:
        sesman = Sesman(None, "1.1.1.1")
        context.getdeco.return_value = deco_time
        context.interdsiplay.return_value = (True, "")

        result = sesman.check_deconnection_time({}, time_for_tests or time)
        assert result == expect_result, f"Expected {expect_result}, got {result}"

        if expect_message:
            context.interdsiplay.assert_called_with({"message": expect_message})
        else:
            context.interdsiplay.assert_not_called()

    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after(
            self, mock_interdsiplay: Mock, mock_getdeco: Mock
    ) -> None:
        base_time = 123456789
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=0, daylight=1, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-15 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-15 15:11:12 (in 2d11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=0, daylight=1, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 2 * (24*3600) - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=3600 // 2, daylight=1, localtime=lambda: struct_time_isdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=-19800, altzone=-19800, daylight=0, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=-3600, altzone=0, daylight=0, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=3600, altzone=0, daylight=0, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )

    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after_utc_2(
            self, mock_interdsiplay: Mock, mock_getdeco: Mock
    ) -> None:
        base_time = 123456789
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=-3600 * 2, altzone=0, daylight=0, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-23 15:11:12",
            expect_result=(base_time, True, ""),
            expect_message="Your session will close at 2024-12-23 15:11:12 (in 10d11h49m)",
            time_for_tests=TimeTest(
                timezone=-3600 * 2, altzone=0, daylight=0, localtime=lambda: struct_time_isnotdst, time=lambda: base_time - 10 * (24*3600) - 11 * 3600 - 49 * 60.0, mktime=lambda x: base_time
            ),
        )

    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_before(
            self, mock_interdsiplay: Mock, mock_getdeco: Mock
    ) -> None:
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-12 15:11:12",
            expect_result=(None, True, ""),  # Should be False ?
            expect_message=None,
        )

    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after2034(
            self, mock_interdsiplay: Mock, mock_getdeco: Mock
    ) -> None:
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2034-12-12 15:11:12",
            expect_result=(None, True, ""),
            expect_message=None,
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="-",
            expect_result=(None, True, ""),
            expect_message=None,
        )
