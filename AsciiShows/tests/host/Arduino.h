#pragma once
// Desktop-only adapter for inspecting the exact firmware renderer, not hardware validation.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
inline uint32_t hostMillis=0;
inline uint32_t millis() { return hostMillis; }
inline void delay(uint32_t ms) { hostMillis+=ms; }
constexpr int OUTPUT=1,LOW=0,HIGH=1;
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
struct HostSerial {
  std::string input;
  void begin(int) {}
  int available() { return int(input.size()); }
  char read() { char c=input.front();input.erase(0,1);return c; }
  void println(const char *s) { puts(s); }
  template<class... T> void printf(const char *f,T... a) { ::printf(f,a...); }
};
inline HostSerial Serial;
