#include <unity.h>

#include "../mocks/app.h"
#include "system_types.h"
#include "temperature_alarm.h"
#include "navigation.h"

extern "C" void setUp(void)
{
}

extern "C" void tearDown(void)
{
}

// ============================================================
// ALARM TESTS
// ============================================================

void test_temperature_below_18_is_low(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::LOW_TEMPERATURE),
        static_cast<int>(evaluateTemperature(17.9f))
    );
}

void test_temperature_exactly_18_is_normal(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(18.0f))
    );
}

void test_temperature_normal_range(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(25.0f))
    );
}

void test_temperature_exactly_30_is_normal(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(30.0f))
    );
}

void test_temperature_above_30_is_high(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::HIGH_TEMPERATURE),
        static_cast<int>(evaluateTemperature(30.1f))
    );
}

// ============================================================
// NAVIGATION TESTS
// ============================================================

void test_navigation_temperature_to_humidity(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::HUMIDITY),
        static_cast<int>(
            NextDisplayMode(DisplayMode::TEMPERATURE)
        )
    );
}

void test_navigation_humidity_to_motion(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::MOTION),
        static_cast<int>(
            NextDisplayMode(DisplayMode::HUMIDITY)
        )
    );
}

void test_navigation_temperature_to_alert_reverse(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::ALERT),
        static_cast<int>(
            PreviousDisplayMode(DisplayMode::TEMPERATURE)
        )
    );
}

void test_navigation_alert_to_light_reverse(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::LIGHT),
        static_cast<int>(
            PreviousDisplayMode(DisplayMode::ALERT)
        )
    );
}

void test_navigation_alert_to_temperature_clockwise(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::TEMPERATURE),
        static_cast<int>(
            NextDisplayMode(DisplayMode::ALERT)
        )
    );
}

// ============================================================
// STATE TESTS
// ============================================================

void test_system_state_active_and_inactive_are_different(void)
{
    TEST_ASSERT_NOT_EQUAL(
        static_cast<int>(SystemState::ACTIVE),
        static_cast<int>(SystemState::INACTIVE)
    );
}

void test_inactivity_timeout_is_15_seconds(void)
{
    TEST_ASSERT_EQUAL(
        15000UL,
        INACTIVITY_TIMEOUT
    );
}

void test_motion_event_bit_is_defined(void)
{
    TEST_ASSERT_NOT_EQUAL(
        0U,
        EVENT_MOTION
    );
}

void test_active_event_bit_is_defined(void)
{
    TEST_ASSERT_NOT_EQUAL(
        0U,
        EVENT_ACTIVE
    );
}

// ============================================================
// ADDITIONAL NAVIGATION TEST
// ============================================================

void test_navigation_alert_to_temperature_reverse_wrap(void)
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::LIGHT),
        static_cast<int>(
            PreviousDisplayMode(DisplayMode::ALERT)
        )
    );
}

// ============================================================
// MAIN TEST RUNNER
// ============================================================

int main(void)
{
    UNITY_BEGIN();

    // Alarm tests: 5
    RUN_TEST(test_temperature_below_18_is_low);
    RUN_TEST(test_temperature_exactly_18_is_normal);
    RUN_TEST(test_temperature_normal_range);
    RUN_TEST(test_temperature_exactly_30_is_normal);
    RUN_TEST(test_temperature_above_30_is_high);

    // Navigation tests: 6
    RUN_TEST(test_navigation_temperature_to_humidity);
    RUN_TEST(test_navigation_humidity_to_motion);
    RUN_TEST(test_navigation_temperature_to_alert_reverse);
    RUN_TEST(test_navigation_alert_to_light_reverse);
    RUN_TEST(test_navigation_alert_to_temperature_clockwise);
    RUN_TEST(test_navigation_alert_to_temperature_reverse_wrap);

    // State tests: 4
    RUN_TEST(test_system_state_active_and_inactive_are_different);
    RUN_TEST(test_inactivity_timeout_is_15_seconds);
    RUN_TEST(test_motion_event_bit_is_defined);
    RUN_TEST(test_active_event_bit_is_defined);

    return UNITY_END();
}