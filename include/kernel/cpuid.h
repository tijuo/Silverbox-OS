#ifndef CPUID_H
#define CPUID_H

struct CPU_Features
{
  unsigned int fpu : 1;
  unsigned int vme : 1;
  unsigned int de : 1;
  unsigned int pse : 1;
  unsigned int tsc : 1;
  unsigned int msr : 1;
  unsigned int pae : 1;
  unsigned int mce : 1;
  unsigned int cx8 : 1;
  unsigned int apic : 1;
  unsigned int resd1 : 1;
  unsigned int sep : 1;
  unsigned int mtrr : 1;
  unsigned int pge : 1;
  unsigned int mca : 1;
  unsigned int cmov : 1;
  unsigned int pat : 1;
  unsigned int pse36 : 1;
  unsigned int psn : 1;
  unsigned int clfl : 1;
  unsigned int resd2 : 1;
  unsigned int dtes : 1;
  unsigned int acpi : 1;
  unsigned int mmx : 1;
  unsigned int fxsr2 : 1;
  unsigned int sse : 1;
  unsigned int sse2 : 1;
  unsigned int ss : 1;
  unsigned int htt : 1;
  unsigned int tm1 : 1;
  unsigned int ia64 : 1;
  unsigned int pbe : 1;
};

struct CPU_Features cpuFeatures;

#endif /* CPUID_H */
