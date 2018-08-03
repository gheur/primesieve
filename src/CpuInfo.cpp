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
#include <primesieve/pmath.hpp>

#include <cstddef>
#include <exception>
#include <string>
#include <vector>

using namespace std;

#if defined(__APPLE__)
  #if !defined(__has_include)
    #define APPLE_SYSCTL
  #elif __has_include(<sys/types.h>) && \
        __has_include(<sys/sysctl.h>)
    #define APPLE_SYSCTL
  #endif
#endif

#if defined(APPLE_SYSCTL)

#include <sys/types.h>
#include <sys/sysctl.h>

namespace {

string getCpuName()
{
  char buffer[256];
  size_t size = sizeof(buffer);
  sysctlbyname("machdep.cpu.brand_string", &buffer, &size, NULL, 0);
  string cpuName = buffer;

  return cpuName;
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

/// Remove all leading and trailing space
/// characters from a string.
///
void trimString(string& str)
{
  string spaceChars = " \f\n\r\t\v";
  size_t pos = str.find_first_not_of(spaceChars);

  if (pos != string::npos)
    str.erase(0, pos);

  reverse(str.begin(), str.end());
  pos = str.find_first_not_of(spaceChars);

  if (pos != string::npos)
    str.erase(0, pos);

  reverse(str.begin(), str.end());
}

} // namespace

#endif

namespace {

/// Get the CPU name using CPUID.
/// Example: Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
/// https://en.wikipedia.org/wiki/CPUID#EAX=80000002h,80000003h,80000004h:_Processor_Brand_String
///
string getCpuName()
{
  string cpuName;

#if defined(IS_X86)

  vector<int> vect;
  int cpuInfo[4] = { 0, 0, 0, 0 };

  cpuId(cpuInfo, 0x80000000);

  // check if CPU name is supported
  if ((unsigned int) cpuInfo[0] >= 0x80000004u)
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

#endif

  return cpuName;
}

} // namespace

#else // Linux

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace {

/// Remove all leading and trailing space
/// characters from a string.
///
void trimString(string& str)
{
  string spaceChars = " \f\n\r\t\v";
  size_t pos = str.find_first_not_of(spaceChars);

  if (pos != string::npos)
    str.erase(0, pos);

  reverse(str.begin(), str.end());
  pos = str.find_first_not_of(spaceChars);

  if (pos != string::npos)
    str.erase(0, pos);

  reverse(str.begin(), str.end());
}

string getString(const string& filename)
{
  ifstream file(filename);
  string str;

  if (file)
  {
    stringstream ss;
    ss << file.rdbuf();
    str = ss.str();

    // remove all space characters
    str.erase(remove_if(str.begin(), str.end(),
      [](unsigned char c) {
        return isspace(c);
    }));
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
      val *= 1024;
    if (str.back() == 'M')
      val *= 1024 * 1024;
    if (str.back() == 'G')
      val *= 1024 * 1024 * 1024;
  }

  return val;
}

vector<string> split(const string& s, char delimiter)
{
   vector<string> tokens;
   string token;
   istringstream tokenStream(s);

   while (getline(tokenStream, token, delimiter))
      tokens.push_back(token);

   return tokens;
}

string getCpuName()
{
  ifstream file("/proc/cpuinfo");
  string notFound;

  if (file)
  {
    // Examples of CPU names:
    // model name : Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
    // Processor  : ARMv7 Processor rev 5 (v7l)
    // cpu        : POWER9 (raw), altivec supported
    //
    regex reg("^(model\\sname|Processor|cpu)\\s+:");
    smatch match;
    string line;
    size_t i = 0;

    while (getline(file, line))
    {
      if (regex_search(line, match, reg))
      {
        size_t pos = match.str().size();
        string cpuName = line.substr(pos);
        trimString(cpuName);
        if (cpuName.find_first_not_of("0123456789") != string::npos)
          return cpuName;
      }

      if (++i > 10)
        break;
    }
  }

  return notFound;
}

/// A thread list file contains a human
/// readable list of thread IDs.
/// https://www.kernel.org/doc/Documentation/cputopology.txt
///
size_t parseThreadList(const string& filename)
{
  size_t threads = 0;
  auto threadList = getString(filename);
  auto tokens = split(threadList, ',');

  // tokens can be:
  // 1) threadId
  // 2) threadId1-threadId2
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

size_t getThreads(const string& threadList, const string& threadMap)
{
  size_t threads = parseThreadList(threadList);

  if (threads != 0)
    return threads;
  else
    return parseThreadMap(threadMap);
}

} // namespace

#endif

namespace primesieve {

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
  return cacheSizes_[2] >= (1 << 12) &&
         cacheSizes_[2] <= (1ull << 40);
}

bool CpuInfo::hasL3Cache() const
{
  return cacheSizes_[3] >= (1 << 12) &&
         cacheSizes_[3] <= (1ull << 40);
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
  return hasL2Sharing() &&
         hasThreadsPerCore() &&
         cacheSharing_[2] <= threadsPerCore_;
}

bool CpuInfo::hasHyperThreading() const
{
  return hasThreadsPerCore() &&
         threadsPerCore_ > 1;
}

#if defined(APPLE_SYSCTL)

void CpuInfo::init()
{
  cpuName_ = getCpuName();

  size_t size1 = sizeof(size_t);
  size_t size2 = sizeof(size_t);
  size_t size3 = sizeof(size_t);

  sysctlbyname("hw.l1dcachesize", &cacheSizes_[1], &size1, NULL, 0);
  sysctlbyname("hw.l2cachesize" , &cacheSizes_[2], &size2, NULL, 0);
  sysctlbyname("hw.l3cachesize" , &cacheSizes_[3], &size3, NULL, 0);

  size_t size = sizeof(cpuCores_);
  sysctlbyname("hw.physicalcpu", &cpuCores_, &size, NULL, 0);
  size_t cpuCores = max<size_t>(1, cpuCores_);

  size = sizeof(cpuThreads_);
  sysctlbyname("hw.logicalcpu", &cpuThreads_, &size, NULL, 0);
  threadsPerCore_ = cpuThreads_ / cpuCores;

  size = 0;

  if (!sysctlbyname("hw.cacheconfig", NULL, &size, NULL, 0))
  {
    size_t n = size / sizeof(size);
    vector<size_t> cacheConfig(n);

    if (cacheConfig.size() > 1)
    {
      // https://developer.apple.com/library/content/releasenotes/Performance/RN-AffinityAPI/index.html
      sysctlbyname("hw.cacheconfig" , &cacheConfig[0], &size, NULL, 0);

      for (size_t i = 1; i < cacheConfig.size(); i++)
        cacheSharing_[i] = cacheConfig[i];
    }
  }
}

#elif defined(_WIN32)

void CpuInfo::init()
{
  cpuName_ = getCpuName();

  typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
  LPFN_GLPI glpi = (LPFN_GLPI) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");

  if (!glpi)
    return;

  DWORD bytes = 0;
  glpi(0, &bytes);

  if (!bytes)
    return;

  size_t size = bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
  vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(size);

  if (!glpi(&info[0], &bytes))
    return;

  for (size_t i = 0; i < size; i++)
  {
    if (info[i].Relationship == RelationProcessorCore)
    {
      cpuCores_++;
      threadsPerCore_ = 0;
      auto mask = info[i].ProcessorMask;

      // ProcessorMask contains one bit set for
      // each logical CPU core related to the
      // current physical CPU core
      while (mask > 0)
      {
        cpuThreads_++;
        threadsPerCore_++;
        mask &= mask - 1;
      }
    }

    if (info[i].Relationship == RelationCache &&
        info[i].Cache.Level >= 1 &&
        info[i].Cache.Level <= 3 &&
        (info[i].Cache.Type == CacheData ||
         info[i].Cache.Type == CacheUnified))
    {
      auto level = info[i].Cache.Level;
      cacheSizes_[level] = info[i].Cache.Size;
    }
  }

// Windows 7 (2009) or later
#if _WIN32_WINNT >= 0x0601

  typedef BOOL (WINAPI *LPFN_GLPIEX)(LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
  LPFN_GLPIEX glpiex = (LPFN_GLPIEX) GetProcAddress(
      GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformationEx");

  if (!glpiex)
    return;

  bytes = 0;
  glpiex(RelationCache, 0, &bytes);

  if (!bytes)
    return;

  vector<char> buffer(bytes);
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* cpu;

  if (!glpiex(RelationCache, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) &buffer[0], &bytes))
    return;

  for (size_t i = 0; i < bytes; i += cpu->Size)
  {
    cpu = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) &buffer[i];

    if (cpu->Relationship == RelationCache &&
        cpu->Cache.GroupMask.Group == 0 &&
        cpu->Cache.Level >= 1 &&
        cpu->Cache.Level <= 3 &&
        (cpu->Cache.Type == CacheData ||
         cpu->Cache.Type == CacheUnified))
    {
      // @warning: GetLogicalProcessorInformationEx() reports
      // incorrect data when Windows is run inside a virtual
      // machine. Specifically the GROUP_AFFINITY.Mask will
      // only have 1 or 2 bits set for each CPU cache (L1, L2 and
      // L3) even if more logical CPU cores share the cache
      auto mask = cpu->Cache.GroupMask.Mask;
      auto level = cpu->Cache.Level;
      auto& cacheSharing = cacheSharing_[level];

      // Cache.GroupMask.Mask contains one bit set for
      // each logical CPU core sharing the cache
      for (; mask > 0; cacheSharing++)
        mask &= mask - 1;
    }
  }

#endif
}

#else

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

  for (int i = 0; i <= 3; i++)
  {
    string path = "/sys/devices/system/cpu/cpu0/cache/index" + to_string(i);
    string cacheLevel = path + "/level";
    string cacheType = path + "/type";

    size_t level = getValue(cacheLevel);
    string type = getString(cacheType);

    if (level >= 1 &&
        level <= 3 &&
        (type == "Data" ||
         type == "Unified"))
    {
      string cacheSize = path + "/size";
      string sharedCpuList = path + "/shared_cpu_list";
      string sharedCpuMap = path + "/shared_cpu_map";
      cacheSizes_[level] = getValue(cacheSize);
      cacheSharing_[level] = getThreads(sharedCpuList, sharedCpuMap);
    }
  }
}

#endif

/// Singleton
const CpuInfo cpuInfo;

} // namespace
