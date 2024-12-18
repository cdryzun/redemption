import os
from unittest import TestCase
from unittest.mock import patch, MagicMock

from freezegun import freeze_time
from datetime import datetime
import time
from collections import namedtuple
from sesmanworker import Sesman


DecoContext = namedtuple("DecoContext", ["getdeco", "interdsiplay"])


def set_test_timezone(timezone: str = 'UTC') -> None:
    os.environ['TZ'] = timezone
    time.tzset()


class TestSesmanCheckDeconnectionTime(TestCase):

    def run_deco(
            self,
            context: DecoContext,
            deco_time: str,
            expect_result: tuple[int, bool, str],
            expect_message: str
    ) -> None:
        sesman = Sesman(None, "1.1.1.1")
        context.getdeco.return_value = deco_time
        context.interdsiplay.return_value = True, ""
        assert sesman.check_deconnection_time({}) == expect_result
        if expect_message:
            context.interdsiplay.assert_called_with(
                {'message': expect_message}
            )
        else:
            context.interdsiplay.assert_not_called()

    @freeze_time("2024-12-13 03:21:34")
    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after(self, mock_interdsiplay, mock_getdeco):
        set_test_timezone('Europe/Paris')
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734099072, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12.",
        )
        set_test_timezone('Japan')
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-13 15:11:12",
            expect_result=(1734070272, True, ""),
            expect_message="Your session will close at 2024-12-13 15:11:12.",
        )

    @freeze_time("2024-12-13 03:21:34")
    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_before(self, mock_interdsiplay, mock_getdeco):
        set_test_timezone('Europe/Paris')
        self.run_deco(
            DecoContext(getdeco=mock_getdeco, interdsiplay=mock_interdsiplay),
            deco_time="2024-12-12 15:11:12",
            expect_result=(None, True, ""),  # Should be False ?
            expect_message=None,
        )

    @freeze_time("2024-12-13 03:21:34")
    @patch("sesmanworker.engine.Engine.get_deconnection_time")
    @patch("sesmanworker.sesman.Sesman.interactive_display_message")
    def test_deconnection_time_after2034(self, mock_interdsiplay, mock_getdeco):
        set_test_timezone('Europe/Paris')
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
