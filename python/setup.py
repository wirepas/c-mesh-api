#!/usr/bin/env python3

import sys
import os
import shutil
from setuptools import Extension, setup
from Cython.Distutils import build_ext as built_ext_cython
from distutils.command.clean import clean

# Package settings
PACKAGE = "wirepas.mesh_api"
DIST_NAME = PACKAGE.replace(".", "-").replace("_", "-")
PACKAGE_DIR = PACKAGE.replace(".", "/")
EXT_NAME = "_mesh_api"
LIB_DIR = "../lib"

# C static library name
if sys.platform == "win32":
    LIB_EXT = ".lib"
else:
    LIB_EXT = ".a"

# Cython extension
ext_1 = Extension(
    PACKAGE + "." + EXT_NAME,
    sources=[PACKAGE_DIR + "/" + EXT_NAME + ".pyx"],
    include_dirs=[PACKAGE_DIR, LIB_DIR + "/api"],
    extra_objects=[LIB_DIR + "/build/mesh_api_lib" + LIB_EXT],
)


def rmdir(dirname):
    """Remove a directory tree with files, relative to current directory"""
    try:
        shutil.rmtree(("./" + dirname).replace("/", os.path.sep))
    except OSError:
        pass


def rm(filename):
    """Remove a file, relative to current directory"""
    try:
        os.remove(("./" + filename).replace("/", os.path.sep))
    except OSError:
        pass


# Custom "clean" command
class clean_custom(clean):
    def run(self):
        # No need to run the original clean command: clean.run(self)

        # Delete build directories
        rmdir(PACKAGE_DIR + "/__pycache__")
        rmdir(DIST_NAME.replace("-", "_") + ".egg-info")
        rmdir("build")
        rmdir("dist")

        # Delete generated C file
        rm(PACKAGE_DIR + "/" + EXT_NAME + ".c")


# Main setup functionality
setup(
    name=DIST_NAME,
    version="0.0",
    author="Wirepas Ltd",
    author_email="techsupport@wirepas.com",
    description="Wirepas Mesh API for Python",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C",
        "Programming Language :: Cython",
        "Programming Language :: Python :: 3 :: Only",
        "Topic :: Communications",
    ],
    url="https://github.com/wirepas/c-mesh-api",
    packages=[PACKAGE],
    ext_modules=[ext_1],
    python_requires=">=3.4",
    cmdclass={"build_ext": built_ext_cython, "clean": clean_custom},
    zip_safe=False,
)
