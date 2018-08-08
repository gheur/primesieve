///
/// @file   CpuInfo.cpp
/// @brief  Get detailed information about the CPU's caches
///         on Windows, macOS and Linux.
///
/// Copyright (C) 2018 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include <primesieve/CpuInfo.hpp>

#include <stdint.h>
#include <cstddef>
#include <exception>
#include <string>
#include <vector>

using namespace std;

namespace primesieve {

/// Singleton (initialized at startup)
const CpuInfo cpuInfo;

CpuInfo::CpuInfo() :
  cpuCores_(0),
  cpuThreads_(0),
  threadsPerCore_(0),
  cacheSizes_{0, 0, 0, 0},
  cacheSharing_{0, 0, 0, 0}
{
  try
  {
    init();
  }
  catch (exception& e)
  {
    // We don't trust the operating system to reliably report
    // all CPU information. In case an unexpected error
    // occurs we continue without relying on CpuInfo and
    // primesieve will fallback to using default CPU settings
    // e.g. 32 KB L1 data cache size.
    error_ = e.what();
  }
}

string CpuInfo::cpuName() const
{
  return cpuName_;
}

size_t CpuInfo::cpuCores() const
{
  return cpuCores_;
}

size_t CpuInfo::cpuThreads() const
{
  return cpuThreads_;
}

size_t CpuInfo::l1CacheSize() const
{
  return cacheSizes_[1];
}

size_t CpuInfo::l2CacheSize() const
{
  return cacheSizes_[2];
}

size_t CpuInfo::l3CacheSize() const
{
  return cacheSizes_[3];
}

size_t CpuInfo::l1Sharing() const
{
  return cacheSharing_[1];
}

size_t CpuInfo::l2Sharing() const
{
  return cacheSharing_[2];
}

size_t CpuInfo::l3Sharing() const
{
  return cacheSharing_[3];
}

size_t CpuInfo::threadsPerCore() const
{
  return threadsPerCore_;
}

string CpuInfo::getError() const
{
  return error_;
}

bool CpuInfo::hasCpuName() const
{
  return !cpuName_.empty();
}

bool CpuInfo::hasCpuCores() const
{
  return cpuCores_ >= 1 &&
         cpuCores_ <= (1 << 20);
}

bool CpuInfo::hasCpuThreads() const
{
  return cpuThreads_ >= 1 &&
         cpuThreads_ <= (1 << 20);
}

bool CpuInfo::hasL1Cache() const
{
  return cacheSizes_[1] >= (1 << 12) &&
         cacheSizes_[1] <= (1 << 30);
}

bool CpuInfo::hasL2Cache() const
{
  uint64_t cacheSize = cacheSizes_[2];

  return cacheSize >= (1 << 12) &&
         cacheSize <= (1ull << 40);
}

bool CpuInfo::hasL3Cache() const
{
  uint64_t cacheSize = cacheSizes_[3];

  return cacheSize >= (1 << 12) &&
         cacheSize <= (1ull << 40);
}

bool CpuInfo::hasL1Sharing() const
{
  return cacheSharing_[1] >= 1 &&
         cacheSharing_[1] <= (1 << 20);
}

bool CpuInfo::hasL2Sharing() const
{
  return cacheSharing_[2] >= 1 &&
         cacheSharing_[2] <= (1 << 20);
}

bool CpuInfo::hasL3Sharing() const
{
  return cacheSharing_[3] >= 1 &&
         cacheSharing_[3] <= (1 << 20);
}

bool CpuInfo::hasThreadsPerCore() const
{
  return threadsPerCore_ >= 1 &&
         threadsPerCore_ <= (1 << 10);
}

bool CpuInfo::hasPrivateL2Cache() const
{
  return hasL2Cache() &&
         hasL2Sharing() &&
         hasThreadsPerCore() &&
         l2Sharing() <= threadsPerCore_;
}

bool CpuInfo::hasHyperThreading() const
{
  return hasThreadsPerCore() &&
         threadsPerCore_ > 1;
}

} // namespace

#if defined(__APPLE__)
  #if !defined(__has_include)
    #define APPLE_SYSCTL
  #elif __has_include(<sys/types.h>) && \
        __has_include(<sys/sysctl.h>)
    #define APPLE_SYSCTL
  #endif
#endif

#if defined(APPLE_SYSCTL)

#include <primesieve/pmath.hpp>
#include <algorithm>
#include <sys/types.h>
#include <sys/sysctl.h>

namespace {

template <typename T>
vector<T> getSysctl(const string& name)
{
  vector<T> res;
  size_t bytes = 0;

  if (!sysctlbyname(name.data(), 0, &bytes, 0, 0))
  {
    using primesieve::ceilDiv;
    size_t size = ceilDiv(bytes, sizeof(T));
    vector<T> vect(size, 0);
    if (!sysctlbyname(name.data(), vect.data(), &bytes, 0, 0))
      res = vect;
  }

  return res;
}

} // namespace

namespace primesieve {

void CpuInfo::init()
{
  auto cpuName = getSysctl<char>("machdep.cpu.brand_string");
  if (!cpuName.empty())
    cpuName_ = cpuName.data();

  auto cpuCores = getSysctl<size_t>("hw.physicalcpu");
  if (!cpuCores.empty())
    cpuCores_ = cpuCores[0];

  auto cpuThreads = getSysctl<size_t>("hw.logicalcpu");
  if (!cpuThreads.empty())
    cpuThreads_ = cpuThreads[0];

  if (hasCpuCores() && hasCpuThreads())
    threadsPerCore_ = cpuThreads_ / cpuCores_;

  // https://developer.apple.com/library/content/releasenotes/Performance/RN-AffinityAPI/index.html
  auto cacheSizes = getSysctl<size_t>("hw.cachesize");
  for (size_t i = 1; i < min(cacheSizes.size(), cacheSizes_.size()); i++)
    cacheSizes_[i] = cacheSizes[i];

  // https://developer.apple.com/library/content/releasenotes/Performance/RN-AffinityAPI/index.html
  auto cacheConfig = getSysctl<size_t>("hw.cacheconfig");
  for (size_t i = 1; i < min(cacheConfig.size(), cacheSharing_.size()); i++)
    cacheSharing_[i] = cacheConfig[i];
}

} // namespace

#elif defined(_WIN32)

#include <windows.h>

#if defined(__i386__) || \
    defined(_M_IX86) || \
    defined(__x86_64__) || \
    defined(_M_X64) || \
    defined(_M_AMD64)
  #define IS_X86
#endif

#if defined(IS_X86)

#include <algorithm>
#include <iterator>

#if defined(_MSC_VER)
  #if !defined(__has_include)
    #include <intrin.h>
    #define MSVC_CPUID
  #elif __has_include(<intrin.h>)
    #include <intrin.h>
    #define MSVC_CPUID
  #endif
#elif defined(__GNUC__) || \
      defined(__clang__)
  #define GNUC_CPUID
#endif

namespace {

/// CPUID is not portable across all x86 CPU vendors and there
/// are many pitfalls. For this reason we prefer to get CPU
/// information from the operating system instead of CPUID.
/// We only use CPUID for getting the CPU name on Windows x86
/// because there is no other way to get that information.
///
void cpuId(int cpuInfo[4], int eax)
{
#if defined(MSVC_CPUID)
  __cpuid(cpuInfo, eax);
#elif defined(GNUC_CPUID)
  int ebx = 0;
  int ecx = 0;
  int edx = 0;

  #if defined(__i386__) && \
      defined(__PIC__)
    // in case of PIC under 32-bit EBX cannot be clobbered
    __asm__ ("movl %%ebx, %%edi;"
             "cpuid;"
             "xchgl %%ebx, %%edi;"
             : "+a" (eax),
               "=D" (ebx),
               "=c" (ecx),
               "=d" (edx));
  #else
    __asm__ ("cpuid;"
             : "+a" (eax),
               "=b" (ebx),
               "=c" (ecx),
               "=d" (edx));
  #endif

  cpuInfo[0] = eax;
  cpuInfo[1] = ebx;
  cpuInfo[2] = ecx;
  cpuInfo[3] = edx;
#else
  // CPUID is not supported
  eax = 0;

  cpuInfo[0] = eax;
  cpuInfo[1] = 0;
  cpuInfo[2] = 0;
  cpuInfo[3] = 0;
#endif
}

/// Remove all leading and trailing
/// space characters.
///
void trimString(string& str)
{
  string spaceChars = " \f\n\r\t\v";
  size_t pos = str.find_first_not_of(spaceChars);
  str.erase(0, pos);

  reverse(str.begin(), str.end());
  pos = str.find_first_not_of(spaceChars);
  str.erase(0, pos);

  reverse(str.begin(), str.end());
}

/// Get the CPU name using CPUID.
/// Example: Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
/// https://en.wikipedia.org/wiki/CPUID#EAX=80000002h,80000003h,80000004h:_Processor_Brand_String
///
string getCpuName()
{
  string cpuName;
  vector<int> vect;
  int cpuInfo[4] = { 0, 0, 0, 0 };

  cpuId(cpuInfo, 0x80000000);

  // check if CPU name is supported
  if ((unsigned) cpuInfo[0] >= 0x80000004u)
  {
    cpuId(cpuInfo, 0x80000002);
    copy_n(cpuInfo, 4, back_inserter(vect));

    cpuId(cpuInfo, 0x80000003);
    copy_n(cpuInfo, 4, back_inserter(vect));

    cpuId(cpuInfo, 0x80000004);
    copy_n(cpuInfo, 4, back_inserter(vect));

    vect.push_back(0);
    cpuName = (char*) vect.data();
    trimString(cpuName);
  }

  return cpuName;
}

} // namespace

#endif

namespace primesieve {

void CpuInfo::init()
{
#if defined(IS_X86)
  cpuName_ = getCpuName();
#endif

  typedef BOOL (WINAPI *LPFN_GLPIEX)(LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);

  LPFN_GLPIEX glpiex = (LPFN_GLPIEX) GetProcAddress(
      GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformationEx");

  // GetLogicalProcessorInformationEx() is supported on Windows 7
  // (2009) or later. So we first check if the user's Windows
  // version supports GetLogicalProcessorInformationEx() before
  // using it. This way primesieve will also run on old Windows
  // versions (though without CPU information).
  if (!glpiex)
    return;

  DWORD bytes = 0;
  glpiex(RelationAll, 0, &bytes);

  if (!bytes)
    return;

  vector<char> buffer(bytes);
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* info;

  if (!glpiex(RelationAll, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) &buffer[0], &bytes))
    return;

  for (size_t i = 0; i < bytes; i += info->Size)
  {
    info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) &buffer[i];

    if (info->Relationship == RelationCache &&
        info->Cache.GroupMask.Group == 0 &&
        info->Cache.Level >= 1 &&
        info->Cache.Level <= 3 &&
        (info->Cache.Type == CacheData ||
         info->Cache.Type == CacheUnified))
    {
      auto level = info->Cache.Level;
      cacheSizes_[level] = info->Cache.CacheSize;

      // @warning: GetLogicalProcessorInformationEx() reports
      // incorrect data when Windows is run inside a virtual
      // machine. Specifically the GROUP_AFFINITY.Mask will
      // only have 1 or 2 bits set for each CPU cache (L1, L2 and
      // L3) even if more logical CPU cores share the cache
      auto mask = info->Cache.GroupMask.Mask;
      cacheSharing_[level] = 0;

      // Cache.GroupMask.Mask contains one bit set for
      // each logical CPU core sharing the cache
      for (; mask > 0; mask &= mask - 1)
        cacheSharing_[level]++;
    }

    if (info->Relationship == RelationProcessorCore)
    {
      cpuCores_++;
      threadsPerCore_ = 0;

      for (size_t j = 0; j < info->Processor.GroupCount; j++)
      {
        auto mask = info->Processor.GroupMask[j].Mask;
        for (; mask > 0; mask &= mask - 1)
          threadsPerCore_++;
      }

      cpuThreads_ += threadsPerCore_;
    }
  }
}

} // namespace

#else // Linux (and all unknown OSes)

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace {

/// Remove all leading and trailing
/// space characters.
///
void trimString(string& str)
{
  string spaceChars = " \f\n\r\t\v";
  size_t pos = str.find_first_not_of(spaceChars);
  str.erase(0, pos);

  reverse(str.begin(), str.end());
  pos = str.find_first_not_of(spaceChars);
  str.erase(0, pos);

  reverse(str.begin(), str.end());
}

void removeAllSpaces(string& str)
{
  str.erase(remove_if(str.begin(), str.end(),
    [](unsigned char c) {
      return isspace(c);
  }), str.end());
}

vector<string> split(const string& str,
                     char delimiter)
{
   vector<string> tokens;
   string token;
   istringstream tokenStream(str);

   while (getline(tokenStream, token, delimiter))
      tokens.push_back(token);

   return tokens;
}

/// Returns the content of a file as a string
/// with all whitespaces removed.
///
string getString(const string& filename)
{
  ifstream file(filename);
  string str;

  if (file)
  {
    stringstream ss;
    ss << file.rdbuf();
    str = ss.str();
    removeAllSpaces(str);
  }

  return str;
}

size_t getValue(const string& filename)
{
  size_t val = 0;
  string str = getString(filename);

  if (!str.empty())
  {
    val = stoul(str);

    // Last character may be:
    // 'K' = kilobytes
    // 'M' = megabytes
    // 'G' = gigabytes
    if (str.back() == 'K')
      val *= 1 << 10;
    if (str.back() == 'M')
      val *= 1 << 20;
    if (str.back() == 'G')
      val *= 1 << 30;
  }

  return val;
}

/// Converts /proc/cpuinfo line into CPU name.
/// Returns an empty string if line does
/// not contain the CPU name.
///
string getCpuName(const string& line)
{
  // Examples of CPU names:
  // model name : Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
  // Processor  : ARMv7 Processor rev 5 (v7l)
  // cpu        : POWER9 (raw), altivec supported
  set<string> cpuLabels
  {
    "model name",
    "Processor",
    "cpu"
  };

  size_t pos = line.find_first_of(':');
  string cpuName;

  if (pos != string::npos)
  {
    string label = line.substr(0, pos);
    trimString(label);
    if (cpuLabels.find(label) != cpuLabels.end())
      cpuName = line.substr(pos + 1);
  }

  return cpuName;
}

bool isValid(const string& cpuName)
{
  if (cpuName.empty())
    return false;
  if (cpuName.find_first_not_of("0123456789") == string::npos)
    return false;

  return true;
}

/// Find the CPU name inside /proc/cpuinfo
string getCpuName()
{
  ifstream file("/proc/cpuinfo");
  string notFound;

  if (file)
  {
    string line;
    size_t i = 0;

    while (getline(file, line))
    {
      string cpuName = getCpuName(line);
      trimString(cpuName);

      if (isValid(cpuName))
        return cpuName;
      if (++i > 10)
        return notFound;
    }
  }

  return notFound;
}

/// A thread list file contains a human
/// readable list of thread IDs.
/// Example: 0-8,18-26
/// https://www.kernel.org/doc/Documentation/cputopology.txt
///
size_t parseThreadList(const string& filename)
{
  size_t threads = 0;
  auto threadList = getString(filename);
  auto tokens = split(threadList, ',');

  for (auto& str : tokens)
  {
    if (str.find('-') == string::npos)
      threads++;
    else
    {
      auto values = split(str, '-');
      auto t0 = stoul(values.at(0));
      auto t1 = stoul(values.at(1));
      threads += t1 - t0 + 1;
    }
  }

  return threads;
}

/// A thread map file contains a hexadecimal
/// or binary string where each set bit
/// corresponds to a specific thread ID.
/// Example: 00000000,00000000,00000000,07fc01ff
/// https://www.kernel.org/doc/Documentation/cputopology.txt
///
size_t parseThreadMap(const string& filename)
{
  size_t threads = 0;
  auto threadMap = getString(filename);
  auto tokens = split(threadMap, ',');

  for (auto& str : tokens)
  {
    for (char c : str)
    {
      string hexChar { c };
      size_t bitmap = stoul(hexChar, nullptr, 16);
      for (; bitmap > 0; threads++)
        bitmap &= bitmap - 1;
    }
  }

  return threads;
}

size_t getThreads(const string& threadList,
                  const string& threadMap)
{
  size_t threads = parseThreadList(threadList);

  if (threads != 0)
    return threads;
  else
    return parseThreadMap(threadMap);
}

} // namespace

namespace primesieve {

/// This works on Linux. We also use this for
/// all unknown OSes, it might work.
///
void CpuInfo::init()
{
  cpuName_ = getCpuName();

  string cpusOnline = "/sys/devices/system/cpu/online";
  cpuThreads_ = parseThreadList(cpusOnline);

  string threadSiblingsList = "/sys/devices/system/cpu/cpu0/topology/thread_siblings_list";
  string threadSiblings = "/sys/devices/system/cpu/cpu0/topology/thread_siblings";
  threadsPerCore_ = getThreads(threadSiblingsList, threadSiblings);

  if (hasCpuThreads() &&
      hasThreadsPerCore())
    cpuCores_ = cpuThreads_ / threadsPerCore_;

  for (size_t i = 0; i <= 3; i++)
  {
    string path = "/sys/devices/system/cpu/cpu0/cache/index" + to_string(i);
    string cacheLevel = path + "/level";
    size_t level = getValue(cacheLevel);

    if (level >= 1 &&
        level <= 3)
    {
      string type = path + "/type";
      string cacheType = getString(type);

      if (cacheType == "Data" ||
          cacheType == "Unified")
      {
        string cacheSize = path + "/size";
        string sharedCpuList = path + "/shared_cpu_list";
        string sharedCpuMap = path + "/shared_cpu_map";
        cacheSizes_[level] = getValue(cacheSize);
        cacheSharing_[level] = getThreads(sharedCpuList, sharedCpuMap);
      }
    }
  }
}

} // namespace

#endif
