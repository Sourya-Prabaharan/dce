"""Exercise master state transitions without requiring Docker."""
import unittest
from unittest.mock import patch
from master import master


class MasterTests(unittest.TestCase):
    def setUp(self):
        master.jobs.clear()
        master.job_queue.clear()
        for worker in master.workers.values():
            worker.update(status="idle", last_heartbeat=0)

    def test_submission_without_workers_is_queued(self):
        self.assertEqual(master.handle_message("SUBMIT job1 8\nprint(1)"), "JOB job1 QUEUED")
        self.assertEqual(master.job_queue, ["job1"])
        self.assertEqual(master.handle_message("STATUS job1"), "STATUS job1 QUEUED")

    def test_completion_during_send_is_not_overwritten(self):
        master.handle_message("HEARTBEAT worker1")
        def complete(_payload):
            return master.handle_message("DONE job1 2\n1\n")
        with patch.object(master.TCPClient, "send", side_effect=complete):
            master.handle_message("SUBMIT job1 8\nprint(1)")
        self.assertEqual(master.jobs["job1"]["status"], "DONE")
        self.assertEqual(master.workers["worker1"]["status"], "idle")
        self.assertEqual(master.handle_message("LOGS job1"), "LOGS job1\n1\n")

    def test_completion_survives_lost_reply(self):
        master.handle_message("HEARTBEAT worker1")
        def complete_then_disconnect(_payload):
            master.handle_message("DONE job1 2\n1\n")
            raise ConnectionResetError
        with patch.object(master.TCPClient, "send", side_effect=complete_then_disconnect):
            master.handle_message("SUBMIT job1 8\nprint(1)")
        self.assertEqual(master.jobs["job1"]["status"], "DONE")
        self.assertEqual(master.job_queue, [])

    def test_unreachable_worker_requeues_submission(self):
        master.handle_message("HEARTBEAT worker1")
        with patch.object(master.TCPClient, "send", side_effect=ConnectionRefusedError):
            self.assertEqual(master.handle_message("SUBMIT job1 8\nprint(1)"), "JOB job1 QUEUED")
        self.assertEqual(master.jobs["job1"]["status"], "QUEUED")
        self.assertIsNone(master.jobs["job1"]["worker"])
        self.assertEqual(master.workers["worker1"]["status"], "dead")


if __name__ == "__main__":
    unittest.main()
