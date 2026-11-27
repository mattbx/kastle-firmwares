#
# MIT License
# Copyright (c) 2026 kastle-firmwares contributors
# Based on Bastl Instruments' Template app, Copyright (c) 2025 Vaclav Mach (Bastl Instruments)
#

# Lasting Damage: a disintegration loop. Build with `make lasting-damage`.
create_kastle2_app(
    APP_NAME "lasting-damage"
    APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/AppLastingDamage.cpp
)
