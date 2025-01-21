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


def set_test_timezone(timezone: str = "UTC") -> None:
    os.environ["TZ"] = timezone
    time.tzset()


set_test_timezone()


class TimeTest(NamedTuple):
    timezone: int
    altzone: int
    daylight: int
    time: Callable[[], float]


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
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC+00:00 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=0, daylight=1, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC-00:30 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=3600 // 2, daylight=1, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC+00:30 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=0, altzone=-3600 // 2, daylight=1, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC+01:00 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=-3600, altzone=0, daylight=0, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
            ),
        )
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC-01:00 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=3600, altzone=0, daylight=0, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
            ),
        )

    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after_utc_2(
            self, mock_interdsiplay: Mock, mock_getdeco: Mock
    ) -> None:
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734102672, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12 UTC+02:00 (in 11h49m)",
            time_for_tests=TimeTest(
                timezone=-3600 * 2, altzone=0, daylight=0, time=lambda: 1734102672 - 11 * 3600 - 49 * 60.0
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
