cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zilch_zephyr_guest)

target_sources(app PRIVATE src/main.c)
