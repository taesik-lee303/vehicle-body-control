/*
 * test_sim_api.c - 브라우저용 API (JSON 상태, 시간 누적)
 * 작성: F (이태식)
 */

#include <string.h>

#include "sim_api.h"
#include "test_util.h"

#define HAS(s) CHECK(strstr(sim_state(), (s)) != NULL)

TEST(initial_state_json)
{
    sim_init(1);
    HAS("{\"t\":0,");
    HAS("\"ign\":0,\"gear\":\"P\",\"speed\":0,");
    HAS("\"doors\":[0,0,0,0]");
    HAS("\"out\":{\"headlight\":0,\"lux_lamp\":0,\"wiper\":\"stop\",\"rain_lamp\":0,\"locked\":0,\"warning\":\"none\"}");
    HAS("\"rx_lost\":0");
}

TEST(exec_and_advance)
{
    sim_init(1);
    CHECK_EQ(sim_exec("drive on"), 0);
    CHECK_EQ(sim_exec("ign on"), 0);
    CHECK_EQ(sim_exec("gear d"), 0);
    CHECK_EQ(sim_exec("pedal 1 0"), 0);
    sim_advance(1000);
    HAS("\"t\":1000,");
    HAS("\"speed\":10,\"speed_mkmh\":10000,");
    HAS("\"drive\":1,\"throttle\":1,\"brake\":0");
    HAS("\"ctx\":{\"rx_lost\":0,\"ign\":1,\"speed_valid\":1");
    CHECK_EQ(sim_exec("nonsense"), 1);
    CHECK_EQ(sim_exec("quit"), 2);
}

/* 브라우저 프레임 간격(예: 16ms)이 50ms로 나누어떨어지지 않아도 시간이 새지 않는다 */
TEST(advance_carries_remainder)
{
    sim_init(1);
    for (int i = 0; i < 60; i++)
        sim_advance(17); /* 1020ms */
    HAS("\"t\":1000,"); /* 20ms는 남아 있음 */
    sim_advance(30);
    HAS("\"t\":1050,");
}

TEST(door_and_switches_in_json)
{
    sim_init(1);
    sim_exec("door rl open");
    sim_exec("light auto");
    sim_exec("wiper high");
    sim_exec("cut lux");
    HAS("\"doors\":[0,0,1,0]");
    HAS("\"light\":\"AUTO\",\"wiper\":\"HIGH\",\"cut\":8,");
}

int main(void)
{
    printf("== 브라우저용 API\n");
    RUN(initial_state_json);
    RUN(exec_and_advance);
    RUN(advance_carries_remainder);
    RUN(door_and_switches_in_json);
    return test_summary();
}
