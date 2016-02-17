from ftest import TestCase
from ftest import TestProcess
import signal


class TestCaseImpl(TestCase):

    def __init__(self):
        TestCase.__init__(self, "test_01")
        self.server = None
        self.client = None

    def ramp_up(self):
        # Create server and client processes with own logger instances
        self.server = TestProcess("./server", self.get_logger("server"))
        self.client = TestProcess("./client", self.get_logger("client"))

    def case(self):
        # Start the client and server
        self.server.start()
        self.client.start()

        # Wait server and client to finish
        self.client.stop(stop_signal=None)
        self.server.stop(stop_signal=signal.SIGINT)

        # Verify events: 1) connection accepted, and 2) connection closed; plus successful termination
        self.server.verify_traces(["New connection: host=::1, port=\d+", "Connection closed\.", "Exit: Success"])

        # Verify data received events (should have received exactly 3 events)
        self.server.verify_traces(["Data received: length=12, data=Hello world!"], min_count=3, max_count=3)

        # Verify connection created event and the successful termination of the program
        self.client.verify_traces(["Connection created.", "Exit: Success"])

        # Verify timer expiry events (should have received exactly 4 expiry events)
        self.client.verify_traces(["Timer expired: next_expiry=0\.421, interval=0\.421, num_expirations=1"], min_count=4, max_count=4)

        # Verify data received events (should have received exactly 3 events, i.e. echo replies back)
        self.client.verify_traces(["Data received: length=12, data=Hello world!"], min_count=3, max_count=3)

    def ramp_down(self):
        pass
