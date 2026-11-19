# Build rules for MinGW portions of SNOW.

cmake_minimum_required(VERSION 4.4.5)

project(SnowProj LANGUAGES C)

add_executable(snowapp
  snowapp.c)
target_include_directories(snowapp PRIVATE
  ../../inc)
