"""Exercise the actual Windows adapter with a deterministic fake engine."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest

EXE = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv)>1 else ''

class StdioTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.media = Path(self.temp.name)/'test.mp4'
        self.media.write_bytes(b'fixture only')
        self.child = subprocess.Popen([EXE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, env={**os.environ,'NOVA_VEYRA_STDIO':'1','QT_QPA_PLATFORM':'offscreen'})
        self.addCleanup(self.cleanup)
        self.lines = queue.Queue()
        def read():
            for line in self.child.stdout:
                self.lines.put(json.loads(line))
        self.reader=threading.Thread(target=read,daemon=True)
        self.reader.start()
        self.wait_for(lambda row:row.get('type')=='nova-ready')

    def cleanup(self):
        if self.child.poll() is None:
            self.child.kill()
        self.child.wait(timeout=5)
        self.child.stdin.close()
        self.reader.join(timeout=2)
        self.child.stdout.close()

    def send(self, cmd, **kwargs):
        self.child.stdin.write((json.dumps({'protocol':1,'cmd':cmd,**kwargs})+'\n').encode())
        self.child.stdin.flush()

    def wait_for(self, predicate):
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            row=self.lines.get(timeout=max(.01,deadline-time.monotonic()))
            if predicate(row):return row
        self.fail('Expected protocol response not received')

    def status(self, **values):
        return self.wait_for(lambda r:r.get('type')=='nova-status' and all(r.get(k)==v for k,v in values.items()))

    def test_open_controls_and_actual_multiplier_choices(self):
        self.send('open_source',source=str(self.media))
        self.status(running=True)
        self.send('pause')
        self.send('gpu_feature',name='fg',enabled=True)
        self.send('fg_multiplier',value=4)
        self.send('subtitle_track',index=0)
        self.status(paused=True,fgEnabled=True,fgMultiplier=4,selectedSubtitle=0)
        self.send('fg_multiplier',value=3) # gap in actual choices, must be rejected
        self.send('subtitle_track',index=1) # unusable track
        self.send('gpu_feature',name='sr',enabled=True)
        self.status(srEnabled=True,fgMultiplier=4,selectedSubtitle=0)
        self.send('quit')
        self.assertEqual(self.child.wait(timeout=5),0)

    def test_live_never_seeks_or_resumes(self):
        self.send('open_source',source='https://example.com/live.m3u8',live=True)
        self.send('resume',seconds=80)
        self.send('seek_by',seconds=20)
        self.send('gpu_feature',name='sr',enabled=True)
        self.status(srEnabled=True,position=0)

    def test_resume_clamped_and_reset_on_new_source(self):
        self.send('open_source',source=str(self.media))
        self.send('resume',seconds=900)
        self.status(position=119)
        self.send('open_source',source=str(self.media))
        self.status(position=0)

    def test_long_batched_input_and_partial_json(self):
        # An ordinary escaped URL can exceed the old 8192-byte JSON limit.
        payload={'protocol':1,'cmd':'open_source','source':'https://example.com/'+('é'*5000)}
        data=(json.dumps(payload)+'\n').encode()
        self.child.stdin.write(data[:11]);self.child.stdin.flush()
        self.child.stdin.write(data[11:]);self.child.stdin.flush()
        self.send('gpu_feature',name='nr',enabled=True)
        self.status(running=True,nrEnabled=True)

    def test_parent_pipe_closure_exits_without_orphan(self):
        self.send('open_source',source=str(self.media))
        self.status(running=True)
        self.child.stdin.close()
        self.assertEqual(self.child.wait(timeout=5),0)

    def test_missing_source_reports_error(self):
        self.send('open_source',source=str(self.media)+'.missing')
        row=self.wait_for(lambda r:r.get('type')=='nova-error')
        self.assertEqual(row['code'],'open_rejected')

if __name__=='__main__':unittest.main(verbosity=2)
