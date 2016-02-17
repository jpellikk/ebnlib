from ftest import TestCase
from ftest import TestProcess


class TestCaseImpl(TestCase):

    def __init__(self):
        TestCase.__init__(self, "test_03")
        self.timers = None

    def ramp_up(self):
        # Create a timer test application instance
        self.timers = TestProcess("./timers", self.get_logger("timers"))

    def case(self):
        # Start the test program
        self.timers.start()

        # Wait the test program to finish
        self.timers.stop(stop_signal=None)

        # Verify events from the periodical timer (Timer1)
        self.timers.verify_traces(["Timer1 expired: next_expiry=1\.999, interval=2\.000, num_expirations=1"], min_count=3, max_count=3)

        # Verify events from the relative timer (Timer2)
        self.timers.verify_traces(["Timer2 expired: next_expiry=0\.000, interval=0\.000, num_expirations=1"], min_count=3, max_count=3)

        # Verify events from the absolute timer (Timer3)
        self.timers.verify_traces(["Timer3 expired: next_expiry=0\.000, interval=0\.000, num_expirations=1"], min_count=3, max_count=3)

        # Verify successful termination of the program
        self.timers.verify_traces(["Exit: Success"])

    def ramp_down(self):
        pass
