# Registers main.c as the entry-point component. ESP-IDF expects a component
# named "main" containing app_main(). On Day 2 we'll add REQUIRES for nvs_flash,
# esp_wifi, mqtt, etc. — kept minimal for the Day 1 blink build.
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
