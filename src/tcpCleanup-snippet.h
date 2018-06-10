// https://github.com/esp8266/Arduino/issues/1923
// https://github.com/esp8266/Arduino/issues/4213
// compatible with lwip-v1 and -v2
// no need for #include

// struct tcp_pcb;
// extern struct tcp_pcb* tcp_tw_pcbs;
// extern "C" void tcp_abort (struct tcp_pcb* pcb);

void tcpCleanup();