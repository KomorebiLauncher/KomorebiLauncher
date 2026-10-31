cmake_minimum_required(VERSION 3.15)
project(optifinerunner Java)
find_package(Java 1.7 REQUIRED COMPONENTS Development)

include(UseJava)

set(CMAKE_JAVA_JAR_ENTRY_POINT komorebilauncher.optifine.Runner)
set(CMAKE_JAVA_COMPILE_FLAGS -target 7 -source 7)

set(SRC
    komorebilauncher/optifine/Runner.java
)

add_jar(OptiFineRunner ${SRC})
install_jar(OptiFineRunner "${JARS_DEST_DIR}")
