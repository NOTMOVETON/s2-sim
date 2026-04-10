from setuptools import find_packages
from setuptools import setup

setup(
    name='s2_msgs',
    version='0.1.0',
    packages=find_packages(
        include=('s2_msgs', 's2_msgs.*')),
)
