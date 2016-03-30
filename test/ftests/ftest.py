"""
Copyright (c) 2016 Jani Pellikka <jpellikk@users.noreply.github.com>

A simple test framework for executing functional tests. The framework
allows for running commands (executables under test) from Python test
scripts and verifying the commands' console output and return code.

The test framework includes file-based logging support.
"""
import subprocess
import threading
import select
import weakref
import signal
import time
import pty
import sys
import os
import re


class TestError(Exception):
    def __init__(self, *args, **kwargs):
        Exception.__init__(self, *args, **kwargs)


class TestLoggerSingleton(object):
     __instance = None

     def __init__(self):
        if self.__instance is not None:
            raise ValueError("Class TestLoggerSingleton already initialized.")
        datetime_stamp = time.strftime("%Y%m%d_%H%M%S", time.localtime())
        self.base_dir = os.getcwd() + "/logs/" + datetime_stamp

     @classmethod
     def get_instance(self):
        if self.__instance is None:
             self.__instance = TestLoggerSingleton()
        return self.__instance


class TestLogger(object):

    def __init__(self, instance, subdir=None, console_output=False):
        logger = TestLoggerSingleton.get_instance()
        self.log_dir = logger.base_dir
        if subdir is not None:
            self.log_dir  = self.log_dir + "/" + subdir
        if not os.path.exists(self.log_dir):
            # Create log directory tree
            os.makedirs(self.log_dir)
        self.log_file = instance + ".log"
        filename = self.log_dir + "/" + self.log_file
        try:
            self.file = open(filename, 'w+')
        except IOError as error:
            raise TestError("Failed to create logfile %s: %s."%(filename, str(error)))
        self.console_output = console_output

    def __del__(self):
        if self.file is not None:
            if not self.file.closed:
                self.file.close()

    def writeline(self, str, line_end="\n"):
        self.write(str + line_end)

    def write(self, str):
        self.file.write(str)
        if self.console_output:
            sys.stdout.write(str)
            sys.stdout.flush()


class TestProcess(object):

    def __init__(self, cmd, logger):
        self.thread = threading.Thread(target=TestProcess._run, args=(weakref.proxy(self),))
        self.pty_master, self.pty_slave = pty.openpty()
        self.logger = logger
        self.process = None
        self.traces = ""
        self.cmd = cmd

    def __del__(self):
        if self.process and self.process.poll() is None:
            # We are going out of scope but stop() hasn't been called:
            #   Kill the running process ungracefully and join the thread
            self.stop(stop_signal=signal.SIGKILL, expected_retcode=None)
        os.close(self.pty_master)
        os.close(self.pty_slave)

    def start(self):
        # Start executing the command in a sub-process
        # Forwarding both STDIN and STDERR to the PTY slave handles
        self.process = subprocess.Popen([self.cmd], stdout=self.pty_slave, stderr=self.pty_slave)
        # Start output reader thread
        self.thread.start()

    def stop(self, stop_signal=signal.SIGTERM, expected_retcode=0):
        if stop_signal is not None and self.process.poll() is None:
            self.process.send_signal(stop_signal)
        # Wait the process to finish
        code = self.process.wait()
        # Wait the reader thread to finish
        if self.thread.is_alive():
            self.thread.join()
        if expected_retcode is not None and code != expected_retcode:
            raise TestError("Unexpected return code: %d."%code)

    def _run(self):
        try:
            while True:
                r, w, x = select.select([self.pty_master], [], [], 0.5)
                if r:
                    char = os.read(self.pty_master, 1024)

                    if char:
                        # To support both Python2 and Python3
                        log_chars = char.decode("ASCII")
                        # Save internally and to the log file
                        self.traces = self.traces + log_chars
                        self.logger.write(log_chars)
                else:
                    if self.process.poll() is not None:
                        break
        except ReferenceError:
            pass

    def verify_traces(self, matched_rows, min_count=1, max_count=None):
        for row in matched_rows:
            count = 0
            for line in self.traces.splitlines():
                if re.match(row, line):
                    count = count + 1
                if count >= min_count and max_count is None:
                    break
            if count < min_count:
                raise TestError("[\"%s\"] was matched less than %d times."%(row, min_count))
            if max_count is not None and count > max_count:
                raise TestError("[\"%s\"] was matched more than %d times."%(row, max_count))

class TestCase(object):
    __logger_instance_name = "main"

    def __init__(self, name):
        self.logger = TestLogger(self.__logger_instance_name, name)
        self.name = name

    def get_logger(self, instance=None):
        # By default return the main logger of this test case
        if instance is None or instance == self.__logger_instance_name:
            return self.logger
        # Create a new logger with the requested instance name
        return TestLogger(instance, self.name)

    def run(self):
        try:
            self.ramp_up()
            self.case()
            self.ramp_down()
        except TestError as error:
            self.logger.writeline(str(error))
            self.logger.writeline("Test %s FAILED!"%self.name)
            sys.stdout.write("F")
            sys.stdout.flush()
        else:
            self.logger.writeline("Test %s PASSED!"%self.name)
            sys.stdout.write(".")
            sys.stdout.flush()

    def ramp_up(self):
        raise ValueError("Function ramp_up() not implemented.")

    def case(self):
        raise ValueError("Function case() not implemented.")

    def ramp_down(self):
        raise ValueError("Function ramp_down() not implemented.")


def run_test_case(cases_dir, case_name):
    try:
        module = __import__(cases_dir + "." + case_name)
        getattr(module, case_name, None).TestCaseImpl().run()
    except ImportError:
        sys.stderr.write("Test case %s not found!\n"%case_name)
        exit(2) # Do NOT ignore a missing test case!

if __name__ == "__main__":
    cases_dir = "cases"
    num_cases = 0
    if len(sys.argv) > 1:
        if sys.argv[1] == "--help":
            sys.stderr.write("Usage: python ftest.py [TC_NAME]...\n")
            exit(1)
        # Each argument defines a test case
        for case_name in sys.argv[1:]:
            run_test_case(cases_dir, case_name)
            num_cases = num_cases + 1
    else:
        # Go through all test cases in the cases directory
        for file in sorted(os.listdir(os.getcwd() + "/" + cases_dir)):
            if not file.startswith("_") and file.endswith(".py"):
                case_name = file[:-3]
                run_test_case(cases_dir, case_name)
                num_cases = num_cases + 1
    if (num_cases > 0):
        sys.stdout.write("\n")
        sys.stdout.write("Executed %d test case(s).\n"%num_cases)
    else:
        sys.stdout.write("No test cases found.\n")
