#ifndef CPUID_H
#define CPUID_H

struct CPU_Features
{
  long fpu : 1;
  long vme : 1;
  long de : 1;
  long pse : 1;
  long tsc : 1;
  long msr : 1;
  long pae : 1;
  long mce : 1;
  long cx8 : 1;
  long apic : 1;
  long resd1 : 1;
  long sep : 1;
  long mtrr : 1;
  long pge : 1;
  long mca : 1;
  long cmov : 1;
  long pat : 1;
  long pse36 : 1;
  long psn : 1;
  long clfl : 1;
  long resd2 : 1;
  long dtes : 1;
  long acpi : 1;
  long mmx : 1;
  long fxsr2 : 1;
  long sse : 1;
  long sse2 : 1;
  long ss : 1;
  long htt : 1;
  long tm1 : 1;
  long ia64 : 1;
  long pbe : 1;
};

struct CPU_Features cpuFeatures;

#endif /* CPUID_H */
