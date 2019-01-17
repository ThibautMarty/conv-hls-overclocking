#include "clkwiz.h"

Settings::Settings(ap_ufixed<11, 8> _clkfbout, ap_uint<8> _divclk_divide, ap_ufixed<11, 8> _clkout0_divide, float _out)
  : clkfbout(_clkfbout)
  , divclk_divide(_divclk_divide)
  , clkout0_divide(_clkout0_divide)
  , out(_out)
{}

float Settings::getPFD() const
{
  return INPUT_CLK / ((float)divclk_divide);
}

float Settings::getVCO() const
{
  return getPFD() * ((float)clkfbout);
}

float Settings::getFrequency() const
{
  return getVCO() / ((float)clkout0_divide);
}

void Clkwiz::memWrite(int addr, ap_uint<32> value)
{
#ifdef ENABLE_CLKWIZ
  // Note: divide addr by 4 because memory is byte-adressed but we have a 32 bits "array"
  ((volatile ap_uint<32>*) mem)[addr >> 2] = value;
#endif // ENABLE_CLKWIZ
}

ap_uint<32> Clkwiz::memRead(int addr)
{
  return ((volatile ap_uint<32>*) mem)[addr >> 2];
}

Clkwiz::Clkwiz(float _minfreq, float _maxfreq, float _step)
  : minfreq(_minfreq)
  , maxfreq(_maxfreq)
  , step(_step)
{
#ifdef ENABLE_CLKWIZ
  int memfd;

  // Map memory to confgure the wizard
  memfd = open("/dev/mem", O_RDWR | O_SYNC);
  if(1 == memfd)
    err(2, "%s on line %d: open()", __FILE__, __LINE__ - 1);

  mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, BASEADDR);
  if(MAP_FAILED == mem)
    err(2, "%s on line %d: mmap()", __FILE__, __LINE__ - 1);

  if(close(memfd) != 0)
    err(2, "%s on line %d: close()", __FILE__, __LINE__ - 1);
#endif // ENABLE_CLKWIZ

  // Compute overclocking settings
  findSettings();

  state = settings.begin();
}

Clkwiz::~Clkwiz()
{
  std::vector<Settings*>::iterator it;

  reset();

#ifdef ENABLE_CLKWIZ
  if(munmap(mem, 4096) != 0)
    err(2, "%s on line %d: munmap()", __FILE__, __LINE__ - 1);
#endif // ENABLE_CLKWIZ

  for(it = settings.begin(); it != settings.end(); it++)
  {
    delete *it;
  }
}

float Clkwiz::restart()
{
  state = settings.begin();

  configure(**state);

  return settings[0]->out;
}

bool Clkwiz::end() const
{
  return settings.end() == state;
}

int Clkwiz::count() const
{
  return settings.size();
}

float Clkwiz::next()
{
  state++;
  if(!end())
  {
    configure(**state);
    return (*state)->out;
  }
  return 0.f;
}

float Clkwiz::previous()
{
  if(settings.begin() != state)
  {
    state--;
    configure(**state);
    return (*state)->out;
  }
  return 0.f;
}

bool Clkwiz::isLocked()
{
#ifdef ENABLE_CLKWIZ
  return memRead(SR) & (1 << LOCKED);
#endif // ENABLE_CLKWIZ
  return 0x01;
}

void Clkwiz::reset()
{
  /*
  while(!isLocked());
  memWrite(CR23, 0x03);
  memWrite(CR23, 0x01);
  while(isLocked());
  //*/
  //*
  memWrite(SRR, 0x0A);
  //*/
}

void Clkwiz::configure(const Settings& settings)
{
  ap_uint<32> reg;
  ap_uint<8> mult;
  ap_uint<10> frac;

  /* std::cerr << settings.out << ": (" << settings.clkfbout << ", " << settings.divclk_divide << ") => 0x"; */

  extract_int_frac(settings.clkfbout, &mult, &frac);
  reg = (frac, mult, settings.divclk_divide);
  memWrite(CR0, reg);

  /* std::cerr << std::hex << reg << std::dec << '\t'; */
  /* std::cerr << "(" << settings.clkout0_divide << ") => 0x"; */

  extract_int_frac(settings.clkout0_divide, &mult, &frac);
  reg = (frac, mult);
  memWrite(CLKOUT0_DIVIDE, reg);

  /* std::cerr << std::hex << reg << std::dec << std::endl; */

  // reload
  while(!isLocked());
  memWrite(CR23, 0x03);
  while(!isLocked());
}

bool settings_cmp(Settings* a, Settings* b)
{
  return a->out < b->out;
}

void extract_int_frac(ap_ufixed<11, 8> fix, ap_uint<8> *integer, ap_uint<10> *frac)
{
  *integer = ap_uint<8>(fix);
  *frac = (ap_uint<10>(fix  << 3) & 0x07) * 125;
}

float ceil8(float value)
{
  return ceil(value * 8.f) / 8.f;
}

float floor8(float value)
{
  return floor(value * 8.f) / 8.f;
}

void Clkwiz::findSettings()
{
#ifdef ENABLE_CLKWIZ
  float clkfbout, divclk_divide, clkout0_divide, vco, pfd, out, goal;
  std::vector<Settings*> tmp;
  std::vector<Settings*>::iterator it;

  // Generate all legal solutions
  for(divclk_divide = ceil(INPUT_CLK / PFDMAX); divclk_divide <= floor(INPUT_CLK / PFDMIN); divclk_divide++)
  {
    pfd = INPUT_CLK / divclk_divide;

    for(clkfbout = ceil8(VCOMIN / pfd); clkfbout <= floor8(VCOMAX / pfd); clkfbout += .125f)
    {
      vco = pfd * clkfbout;

      for(clkout0_divide = ceil8(vco / maxfreq); clkout0_divide <= floor8(vco / minfreq); clkout0_divide += .125f)
      {
        out = vco / clkout0_divide;
        tmp.push_back(new Settings(clkfbout, divclk_divide, clkout0_divide, out));
      }
    }
  }

  // Sort them according to result frequencies
  std::sort(tmp.begin(), tmp.end(), settings_cmp);

  // Select one solution by step
  settings.reserve(1 + (maxfreq - minfreq) / step);
  goal = minfreq;
  for(it = tmp.begin(); it != tmp.end(); it++)
  {
    if((*it)->out >= goal)
    {
      settings.push_back(*it);
      goal += step;
    }
    else
    {
      delete *it;
    }
  }
#else // ENABLE_CLKWIZ
  for(int i = 0; i < int((maxfreq - minfreq + step) / step); i++)
  {
    settings.push_back(new Settings(1, 1, 1, minfreq + step * float(i)));
  }
#endif //ENABLE_CLKWIZ
}
