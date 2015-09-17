import conjecture._bindings as raw
import traceback
import os
import signal

class ContextHasAborted(Exception):
    pass

class ExampleRejected(BaseException):
    pass

class ExampleFailed(Exception):
    pass

@raw.ffi.callback('int64_t(void*)')
def python_friendly_forker(ignored):
    return os.fork()

CONJECTURE_CONFIG_OPTIONS = {
    'max_examples', 'max_buffer_size', 'suppress_output'
}

class TestRunner(object):
    def __init__(self, fork=False, **kwargs):
        self.lib = raw.lib
        self.runner = raw.ffi.new('conjecture_runner*')
        self.lib.conjecture_runner_init(self.runner)
        self.runner.fork = python_friendly_forker
        self.runner.abort_on_fail = fork
        for k, v in kwargs.items():
            if k in CONJECTURE_CONFIG_OPTIONS:
                setattr(self.runner, k, v)            
            
    def __del__(self):
        self.lib.conjecture_runner_release(self.runner)


    def run_test(self, test, *args, **kwargs):
        parent = os.getpid()

        @raw.ffi.callback(
            "void(conjecture_context*, void*)"
        )
        def runtest(context, _):
            try:
                test(TestContext(self, context), *args, **kwargs)
            except SystemExit as e:
                os._exit(e.code)
            except ExampleRejected:
                os._exit(0)
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
                raw.lib.conjecture_context_init_from_buffer(
                    context, self.runner, buf)
                test(TestContext(self, context), *args, **kwargs)
            finally:
                raw.lib.conjecture_buffer_del(buf)
        

class TestContext(object):
    def __init__(self, runner, c_context):
        self.runner = runner
        self.c_context = c_context


PREFIX = 'conjecture_'

def wrap_conjecture_function(name):
    assert name.startswith(PREFIX)
    underlying = getattr(raw.lib, name)
    clean_name = name[len(PREFIX):]

    def accept(context, *args):
        def function_string():
            return "%s(%s)" % (
                clean_name, ', '.join(map(repr, args))
            )
        if raw.lib.conjecture_is_aborted(context.c_context):
            raise ContextHasAborted(
                "Illegal call to %s with aborted TestContext" % (clean_name,))
        result = underlying(context.c_context, *args)
        if raw.lib.conjecture_is_aborted(context.c_context):
            if context.c_context.status == raw.lib.CONJECTURE_TEST_FAILED:
                raise ExampleFailed("Call failed in call to %s" % (
                    function_string()
                ))
            if context.c_context.status == raw.lib.CONJECTURE_DATA_REJECTED:
                raise ExampleRejected("Call rejected in call to %s" % (
                    function_string()
                ))
            assert False
        return result
    accept.__name__ = clean_name
    accept.__qualname__ = 'conjecture.bindings.%s' % (clean_name,)
    globals()[clean_name] = accept

wrap_conjecture_function('conjecture_assume')
wrap_conjecture_function('conjecture_reject')

for name in dir(raw.lib):
    if name.startswith('conjecture_draw'):
        wrap_conjecture_function(name)
