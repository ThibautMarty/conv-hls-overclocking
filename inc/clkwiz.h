#ifndef __CLKWIZ_H
#define __CLKWIZ_H

#include <sys/mman.h> // mmap
#include <sys/stat.h> // open
#include <fcntl.h>    // open
#include <unistd.h>   // close
#include <err.h>      // err
#include <cmath>      // ceil, floor
#include <vector>
#include <algorithm>  // std::sort
#include "options.sw.h"
#include "ap_int.h"

// Clocking wizard configuration base address
#define BASEADDR  0x43C00000

// Note: all frequencies are in MHz

// Frequency bounds for MMCM mode (ds191) + margin for PFDMIN
#define VCOMIN 600.f
#define VCOMAX 1440.f
#define PFDMIN 25.f // 10.f
#define PFDMAX 450.f // 500.F

// Registers (pg065)
#define SRR            0
#define SR             0x04
#define CR0            0x200
#define CLKOUT0_DIVIDE 0x208
#define CR23           0x25C

// Bits
#define LOCKED           0
#define CLKFBOUT_MULT    8
#define CLKFBOUT_FRAC    16
#define CLKOUT0_FRAC     8

// Configuration settings parameters
struct Settings
{
  const ap_ufixed<11, 8> clkfbout;
  const ap_uint<8> divclk_divide;
  const ap_ufixed<11, 8> clkout0_divide;
  const float out;
  Settings(ap_ufixed<11, 8> _clkfbout, ap_uint<8> _divclk_divide, ap_ufixed<11, 8> _clkout0_divide, float _out);
  float getPFD() const;
  float getVCO() const;
  float getFrequency() const;
};

class Clkwiz
{
  private:
    void *mem;

    void memWrite(int addr, ap_uint<32> value);
    ap_uint<32> memRead(int addr);

    std::vector<Settings*> settings;
    void findSettings();

    float minfreq;
    float maxfreq;
    float step;
    std::vector<Settings*>::iterator state; // current settings

    bool isLocked();
    void reset();
    void configure(const Settings& settings);

  public:
    Clkwiz(float _minfreq, float _maxfreq, float _step);
    virtual ~Clkwiz();

    float restart();  // configure with the first frequency (and return it)
    float next();     // configure with the next frequency (and return it) /!\ test end() before
    float previous(); // configure with the previous frequency (and return it) /!\ test against settings.begin() before
    bool end() const; // test if we passed the end
    int count() const;// return number of possible clocks
};

// Helper functions
bool settings_cmp(Settings* a, Settings* b);
float ceil8(float value); // ceil to multiple of .125
float floor8(float value); // floor to multiple of .125
void extract_int_frac(ap_ufixed<11, 8> fix, ap_uint<8> *integer, ap_uint<10> *frac);

#endif // __CLKWIZ_H
