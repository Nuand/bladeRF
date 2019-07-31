"""A setuptools based setup module.

See:
https://packaging.python.org/en/latest/distributing.html
https://github.com/pypa/sampleproject
"""

# Always prefer setuptools over distutils
from setuptools import setup, find_packages
# To use a consistent encoding
from codecs import open
from os import path

here = path.abspath(path.dirname(__file__))

# Get the long description from the README file
with open(path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

setup(
    name='bladerf',
    version='1.2.0',
    description='CFFI-based Python 3 binding to libbladeRF',
    long_description=long_description,
    url='https://github.com/Nuand/bladeRF',
    author='Nuand LLC',
    author_email='bladerf@nuand.com',
    license='MIT',
    python_requires='>=3',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: End Users/Desktop',
        'Intended Audience :: Science/Research',
        'Intended Audience :: Telecommunications Industry',
        'Topic :: Communications :: Ham Radio',
        'Topic :: Scientific/Engineering',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
    ],
    keywords='bladerf sdr cffi radio libbladerf',
    packages=find_packages(exclude=['contrib', 'docs', 'tests']),
    install_requires=['cffi'],
    entry_points={
        'console_scripts': [
            'bladerf-tool=bladerf._tool:main',
        ],
    },
    project_urls={
        'Bug Reports': 'https://github.com/Nuand/bladeRF/issues',
        'Source': 'https://github.com/Nuand/bladeRF',
        'Nuand': 'https://nuand.com',
    },
)
