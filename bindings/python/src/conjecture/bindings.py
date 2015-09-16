import conjecture._bindings as raw
import traceback
import os
import signal


@raw.ffi.callback('int64_t(void*)')
def python_friendly_forker(ignored):
    return os.fork()

class TestRunner(object):
    def __init__(self):
        self.lib = raw.lib
        self.runner = raw.ffi.new('conjecture_runner*')
        self.lib.conjecture_runner_init(self.runner)
        self.runner.fork = python_friendly_forker
    def __del__(self):
        self.lib.conjecture_runner_release(self.runner)


    def run_test(self, test, *args, **kwargs):
        parent = os.getpid()

        @raw.ffi.callback(
            "void(conjecture_context*, void*)"
        )
        def runtest(context, _):
            try:
                test(context, *args, **kwargs)
            except SystemExit as e:
                os._exit(e.code)
            except KeyboardInterrupt:
                os.kill(parent, signal.SIGINT)
                os._exit(0)
            except BaseException:
                traceback.print_exc()
                os._exit(1)
        buf = raw.lib.conjecture_run_test_for_buffer(
            self.runner, runtest, raw.ffi.NULL
        )
        if buf != raw.ffi.NULL:
            try:
                context = raw.ffi.new('conjecture_context*')
                raw.lib.conjecture_context_init_from_buffer(context, buf)
                test(context, *args, **kwargs)
            finally:
                raw.lib.conjecture_buffer_del(buf)
        


PREFIX = 'conjecture_'

def wrap_conjecture_function(name):
    assert name.startswith(PREFIX)
    underlying = getattr(raw.lib, name)
    def accept(*args):
        return underlying(*args)
    clean_name = name[len(PREFIX):]
    accept.__name__ = clean_name
    accept.__qualname__ = 'conjecture.bindings.%s' % (clean_name,)
    globals()[clean_name] = accept

wrap_conjecture_function('conjecture_assume')
wrap_conjecture_function('conjecture_reject')

for name in dir(raw.lib):
    if name.startswith('conjecture_draw'):
        wrap_conjecture_function(name)
