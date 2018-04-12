#!/usr/bin/env python3

from setuptools import setup, find_packages, Extension

with open('README.rst') as readme:
    long_description = readme.read()

setup(
    name = 'dcl2',
    version = '0.0.1',
    description = 'DeadCom Layer 2 protocol implementation',
    long_description = long_description,
    classifiers = [
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Communications"
    ],
    keywords = [
        'dcl2',
        'deadlock',
        'libdeadcom'
    ],
    author = 'Adam Dej',
    author_email = 'dejko.a@gmail.com',
    url = 'https://github.com/fmfi-svt-deadlock/libdeadcom',
    license = 'MIT',
    packages = find_packages(),
    ext_modules = [
        Extension(
            name = 'dcl2._dcl2',
            language = 'c',
            sources = [
                'dcl2/_dcl2.c'
            ],
            extra_compile_args = [
                '-I../lib/inc'
            ],
            libraries = ['dcl2'],
            library_dirs = ['../../build']
        )
    ],
    include_package_data = True,
    zip_safe = False
)
