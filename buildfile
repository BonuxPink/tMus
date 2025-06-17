./: {*/ -build/ -submodules/ -bin/ -assets/ -obj/} doc{README.org} manifest

# Don't install tests.
#
tests/: install = false
