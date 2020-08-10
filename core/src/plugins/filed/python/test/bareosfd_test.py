import unittest
import bareosfd
import time


class TestBareosFd(unittest.TestCase):

    def test_SetValue(self):
        self.assertRaises(RuntimeError, bareosfd.SetValue, 2)

    def test_DebugMessage(self):
        self.assertRaises(RuntimeError, bareosfd.DebugMessage, "This is a debug message")

    def test_RestoreObject(self):
        test_RestoreObject = bareosfd.RestoreObject()
        self.assertEqual(
            'RestoreObject(object_name="", object="", plugin_name="<NULL>", object_type=0, object_len=0, object_full_len=0, object_index=0, object_compression=0, stream=0, jobid=0)',
            str(test_RestoreObject),
        )

        #r2 = bareosfd.RestoreObject()
        #r2.object_name="this is a very long object name"
        #r2.object="123456780"
        ##r2.plugin_name="this is a plugin name"
        #r2.object_type=3
        #r2.object_len=111111
        #r2.object_full_len=11111111
        #r2.object_index=1234
        #r2.object_compression=1
        #r2.stream=4
        #r2.jobid=123123
        #print (r2)
        #self.assertEqual(
        #   'RestoreObject(object_name="this is a very long object name", object="", plugin_name="<NULL>", object_type=3, object_len=111111, object_full_len=11111111, object_index=1234, object_compression=1, stream=4, jobid=123123)',
        #    str(test_RestoreObject),
        #)



    def test_StatPacket(self):
        timestamp = time.time()
        test_StatPacket = bareosfd.StatPacket()

        # check that the initialization of timestamps from current time stamp works
        self.assertAlmostEqual(test_StatPacket.atime, timestamp, delta=1)
        self.assertAlmostEqual(test_StatPacket.mtime, timestamp, delta=1)
        self.assertAlmostEqual(test_StatPacket.ctime, timestamp, delta=1)

        # set fixed values for comparison
        test_StatPacket.atime=999
        test_StatPacket.mtime=1000
        test_StatPacket.ctime=1001
        self.assertEqual(
            "StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=-1, atime=999, mtime=1000, ctime=1001, blksize=4096, blocks=1)",
            str(test_StatPacket),
        )
        sp2 = bareosfd.StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=-1, atime=1, mtime=1, ctime=1, blksize=4096, blocks=1)
        self.assertEqual('StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=-1, atime=1, mtime=1, ctime=1, blksize=4096, blocks=1)', str(sp2))

    def test_SavePacket(self):
        test_SavePacket = bareosfd.SavePacket()
        self.assertEqual(
            'SavePacket(fname="", link="", type=0, flags=<NULL>, no_read=0, portable=0, accurate_found=0, cmd="<NULL>", save_time=0, delta_seq=0, object_name="", object="", object_len=0, object_index=0)',
            str(test_SavePacket),
        )

    def test_RestorePacket(self):
        test_RestorePacket = bareosfd.RestorePacket()
        self.assertEqual(
            'RestorePacket(stream=0, data_stream=0, type=0, file_index=0, linkFI=0, uid=0, statp="<NULL>", attrEx="<NULL>", ofname="<NULL>", olname="<NULL>", where="<NULL>", RegexWhere="<NULL>", replace=0, create_status=0)',
            str(test_RestorePacket),
        )

    def test_IoPacket(self):
        test_IoPacket = bareosfd.IoPacket()
        self.assertEqual(
            'IoPacket(func=0, count=0, flags=0, mode=0000, buf="", fname="<NULL>", status=0, io_errno=0, lerror=0, whence=0, offset=0, win32=0)',
            str(test_IoPacket),
        )

    def test_AclPacket(self):
        test_AclPacket = bareosfd.AclPacket()
        self.assertEqual('AclPacket(fname="<NULL>", content="")', str(test_AclPacket))


    def test_XattrPacket(self):
        test_XattrPacket = bareosfd.XattrPacket()
        self.assertEqual(
            'XattrPacket(fname="<NULL>", name="", value="")', str(test_XattrPacket)
        )


if __name__ == "__main__":
    unittest.main()
