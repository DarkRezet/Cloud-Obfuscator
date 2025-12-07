from setuptools import setup, Extension

setup(
    name="keymodule",
    version="2.1",
    ext_modules=[
        Extension(
            "keymodule",
            ["keymodule.c"],
            libraries=["Advapi32"]
        )
    ]
)
