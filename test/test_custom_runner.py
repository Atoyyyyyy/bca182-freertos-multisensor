from platformio.public import UnityTestRunner


class CustomTestRunner(UnityTestRunner):
    EXTRA_LIB_DEPS = None

    def configure_build_env(self, env):
        pass