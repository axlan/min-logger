#!/usr/bin/env python3

# To install completions see: https://jsonargparse.readthedocs.io/en/v4.40.1/#shtab

# example.py --print_shtab=bash > /etc/bash_completion.d/example
# eval "$(example.py --print_shtab=bash)"

from typing import Optional
from jsonargparse import ArgumentParser


def main():
    parser = ArgumentParser()
    parser.add_argument("--bool", type=Optional[bool])

    parser.parse_args()


if __name__ == "__main__":
    main()
