from ftest import TestCase
from ftest import TestProcess
import signal


class TestCaseImpl(TestCase):

    def __init__(self):
        TestCase.__init__(self, "test_02")
        self.server1 = None
        self.server2 = None
        self.client1 = None

    def ramp_up(self):
        # Create server and client processes with own logger instances
        self.server1 = TestProcess("./server", self.get_logger("server1"))
        self.server2 = TestProcess("./server", self.get_logger("server2"))
        self.client1 = TestProcess("./client", self.get_logger("client1"))

    def case(self):
        # Start client1
        self.client1.start()

        # Wait the client1 to finish
        self.client1.stop(stop_signal=None)

        # Verify connection error event
        self.client1.verify_traces(["Connection failed\.", "Exit: Success"])

        # Start server1
        self.server1.start()

        # Attempt to start server2
        self.server2.start()

        # Stop server1 and verify return code (1)
        self.server2.stop(stop_signal=None, expected_retcode=1)

        # Verify that bind() call fails; address is already in use
        self.server2.verify_traces(["bind\(\): Address already in use", "Creating socket failed\.", "Exit: Failure"])

        # Stop server1
        self.server1.stop(stop_signal=signal.SIGINT)

    def ramp_down(self):
        pass
