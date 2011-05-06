/*
 * PrimeSieve.cpp -- This file is part of primesieve
 *
 * Copyright (C) 2011 Kim Walisch, <kim.walisch@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "PrimeSieve.h"
#include "ParallelPrimeSieve.h"
#include "settings.h"
#include "pmath.h"
#include "ResetSieve.h"
#include "PrimeNumberFinder.h"
#include "PrimeNumberGenerator.h"

#include <stdint.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <ctime>

PrimeSieve::PrimeSieve() :
    startNumber_(0), stopNumber_(0),
    sieveSize_(settings::DEFAULT_SIEVESIZE_PRIMENUMBERFINDER), flags_(
    COUNT_PRIMES), timeElapsed_(0.0), callback(NULL) {
  parent_ = this;
  this->reset();
}

uint64_t PrimeSieve::getStartNumber() const {
  return startNumber_;
}

uint64_t PrimeSieve::getStopNumber() const {
  return stopNumber_;
}

uint32_t PrimeSieve::getSieveSize() const {
  return sieveSize_ / 1024;
}

uint32_t PrimeSieve::getFlags() const {
  return flags_;
}

/**
 * Generate the prime numbers between startNumber and stopNumber and
 * call a callback function for each prime.
 */
void PrimeSieve::generatePrimes(uint64_t startNumber, uint64_t stopNumber,
    void (*callback)(uint64_t, void*), void* cbObj = NULL) {
  if (callback == NULL)
    throw std::invalid_argument("callback must not be NULL");
  this->setStartNumber(startNumber);
  this->setStopNumber(stopNumber);
  this->setFlags(CALLBACK_PRIMES);
  this->callback = callback;
  this->cbObj_ = cbObj;
  this->sieve();
}

/**
 * Get the count of prime numbers between startNumber and stopNumber.
 */
uint64_t PrimeSieve::getPrimeCount(uint64_t startNumber, uint64_t stopNumber) {
  this->setStartNumber(startNumber);
  this->setStopNumber(stopNumber);
  this->setFlags(COUNT_PRIMES);
  this->sieve();
  return this->getPrimeCount();
}

/**
 * Get the count of prime numbers between startNumber and stopNumber
 * after having called sieve().
 */
uint64_t PrimeSieve::getPrimeCount() const {
  return counts_[0];
}

/**
 * Get the count of twin primes between startNumber and stopNumber
 * after having called sieve().
 */
uint64_t PrimeSieve::getTwinCount() const {
  return counts_[1];
}

/**
 * Get the count of prime triplets between startNumber and stopNumber
 * after having called sieve().
 */
uint64_t PrimeSieve::getTripletCount() const {
  return counts_[2];
}

/**
 * Get the count of prime quadruplets between startNumber and
 * stopNumber after having called sieve().
 */
uint64_t PrimeSieve::getQuadrupletCount() const {
  return counts_[3];
}

/**
 * Get the count of prime quintuplets between startNumber and
 * stopNumber after having called sieve().
 */
uint64_t PrimeSieve::getQuintupletCount() const {
  return counts_[4];
}

/**
 * Get the count of prime sextuplets between startNumber and
 * stopNumber after having called sieve().
 */
uint64_t PrimeSieve::getSextupletCount() const {
  return counts_[5];
}

/**
 * Get the count of prime septuplets between startNumber and
 * stopNumber after having called sieve().
 */
uint64_t PrimeSieve::getSeptupletCount() const {
  return counts_[6];
}

/**
 * Get the count of prime numbers or prime k-tuplets between
 * startNumber and stopNumber after having called sieve().
 * e.g type = 0 for prime numbers,
 *     type = 1 for twin primes,
 *     type = 2 for prime triplets,
 *     ...
 * @param type <= 7
 */
uint64_t PrimeSieve::getCounts(uint32_t type) const {
  if (type >= COUNTS_SIZE)
    throw std::out_of_range("getCounts(uint32_t) type out of range");
  return counts_[type];
}

/**
 * Get the time elapsed in seconds of the last sieve session.
 */
double PrimeSieve::getTimeElapsed() const {
  return timeElapsed_;
}

/**
 * Set a start number for sieving.
 * @pre startNumber < (2^64-1) - (2^32-1) * 10
 */
void PrimeSieve::setStartNumber(uint64_t startNumber) {
  // EratBig and EratMedium stopNumber limit
  if (startNumber >= UINT64_MAX - UINT32_MAX * UINT64_C(10))
    throw std::invalid_argument("START must be < (2^64-1) - (2^32-1) * 10");
  startNumber_ = startNumber;
}

/**
 * Set a stop number for sieving.
 * @pre stopNumber < (2^64-1) - (2^32-1) * 10
 */
void PrimeSieve::setStopNumber(uint64_t stopNumber) {
  // EratBig and EratMedium stopNumber limit
  if (stopNumber >= UINT64_MAX - UINT32_MAX * UINT64_C(10))
    throw std::invalid_argument("STOP must be < (2^64-1) - (2^32-1) * 10");
  stopNumber_ = stopNumber;
}

/**
 * Set the size (in KiloBytes) of the sieve of Eratosthenes array.
 * The best performance is achieved with a sieve size that matches
 * the CPU's L1 cache size (usually 32 or 64 KB) when sieving < 10^14
 * and a sieve size of your CPU's L2 cache size above.
 *
 * Default sieveSize = 64 KiloBytes
 *
 * @pre sieveSize must be a power of 2,
 *      sieveSize >= 1 KiloByte,
 *      sieveSize <= 8192 KiloBytes.
 */
void PrimeSieve::setSieveSize(uint32_t sieveSize) {
  // SieveOfEratosthenes lower sieve size limit AND 
  // EratBig upper sieveSize limit
  if (sieveSize < 1 || sieveSize > 8192)
    throw std::invalid_argument("sieve size must be >= 1 and <= 8192 KiloBytes");
  // EratBig requires a power of 2 sieve size
  if (!isPowerOf2(sieveSize))
    throw std::invalid_argument("sieve size must be a power of 2");
  // convert to Bytes
  sieveSize_ = sieveSize * 1024;
}

/**
 * Set the flags (settings) of PrimeSieve.
 * @param flags
 *   COUNT_PRIMES      OR (bitwise '|')
 *   COUNT_TWINS       OR
 *   COUNT_TRIPLETS    OR
 *   COUNT_QUADRUPLETS OR
 *   COUNT_QUINTUPLETS OR
 *   COUNT_SEXTUPLETS  OR
 *   COUNT_SEPTUPLETS  OR
 *   PRINT_PRIMES      OR
 *   PRINT_TWINS       OR
 *   PRINT_TRIPLETS    OR
 *   PRINT_QUADRUPLETS OR
 *   PRINT_QUINTUPLETS OR
 *   PRINT_SEXTUPLETS  OR
 *   PRINT_SEPTUPLETS  OR
 *   PRINT_STATUS.
 */
void PrimeSieve::setFlags(uint32_t flags) {
  flags_ = flags;
}

/**
 * For use with ParallelPrimeSieve.
 * @see ParallelPrimeSieve.cpp
 */
void PrimeSieve::setChildPrimeSieve(uint64_t startNumber, uint64_t stopNumber,
    ParallelPrimeSieve* parent) {
  parent_ = parent;
  startNumber_ = startNumber;
  stopNumber_ = stopNumber;
  this->setSieveSize(parent_->getSieveSize());
  this->setFlags(parent_->getFlags());
}

void PrimeSieve::reset() {
  segments_ = 0;
  for (uint32_t i = 0; i < COUNTS_SIZE; i++)
    counts_[i] = 0;
  status_ = -1.0;
  parent_->doStatus(0);
}

/**
 * Print the status (in percent) of the sieving process
 * to the standard output.
 */
void PrimeSieve::doStatus(uint64_t segment) {
  segments_ += segment;
  double old = status_;
  status_ = static_cast<double> (segments_) /
            static_cast<double> (1 + stopNumber_ - startNumber_) * 100.0;
  if (static_cast<int> (status_) > 99)
    status_ = 100.0;
  if ((flags_ & PRINT_STATUS) &&
      static_cast<int> (status_) > static_cast<int> (old)) {
    std::ostringstream os;
    os << '\r' << static_cast<int> (status_) << '%';
    std::cout << os.str() << std::flush;
  }
}

void PrimeSieve::doSmallPrime(uint32_t low, uint32_t high, uint32_t type, 
    std::string prime) {
  if (startNumber_ <= low && stopNumber_ >= high) {
    if (flags_ & (COUNT_PRIMES << type))
      counts_[type]++;
    if (flags_ & (PRINT_PRIMES << type))
      std::cout << prime << std::endl;
    if (flags_ & CALLBACK_PRIMES)
      this->callback(prime[0]-'0', cbObj_);
  }
}

/**
 * Sieve the prime numbers and/or prime k-tuplets between startNumber
 * and stopNumber.
 */
void PrimeSieve::sieve() {
  if (stopNumber_ < startNumber_)
    throw std::invalid_argument("STOP must be >= START");
  timeElapsed_ = static_cast<double> (std::clock());
  this->reset();

  // small primes have to be examined manually
  if (startNumber_ <= 5) {
    this->doSmallPrime(2,  2, 0, "2");
    this->doSmallPrime(3,  3, 0, "3");
    this->doSmallPrime(5,  5, 0, "5");
    this->doSmallPrime(3,  5, 1, "(3, 5)");
    this->doSmallPrime(5,  7, 1, "(5, 7)");
    this->doSmallPrime(5, 11, 2, "(5, 7, 11)");
    this->doSmallPrime(5, 13, 3, "(5, 7, 11, 13)");
    this->doSmallPrime(5, 17, 4, "(5, 7, 11, 13, 17)");
  }

  if (stopNumber_ >= 7) {
    // needed by primeNumberGenerator and primeNumberFinder to
    // reset their sieve arrays
    ResetSieve resetSieve(settings::PREELIMINATE_RESETSIEVE);
    // used to sieve the prime numbers and prime k-tuplets between
    // startNumber_ and stopNumber_
    PrimeNumberFinder primeNumberFinder(
        (startNumber_ > 7) ? startNumber_ : 7,
        stopNumber_, 
        sieveSize_, 
        flags_, 
        &resetSieve, 
        parent_);

    if (U32SQRT(stopNumber_) > resetSieve.getEliminateUpTo()) {
      // used to generate the prime numbers up to sqrt(stopNumber_)
      // needed for sieving by primeNumberFinder
      PrimeNumberGenerator primeNumberGenerator(
          settings::SIEVESIZE_PRIMENUMBERGENERATOR,
          &primeNumberFinder);
      std::vector<uint32_t> primes16Bit;
      primes16Bit.push_back(3);
      uint32_t stop = U32SQRT(primeNumberGenerator.getStopNumber());
      uint32_t keep = U32SQRT(stop);
      // the following trial division algorithm is used to generate the
      // prime numbers up to stopNumber_^0.25 needed for sieving by
      // primeNumberGenerator. Although the algorithm is never
      // used > 65536 it finds the prime numbers up to 10^7 in 1 second
      // on an Intel Core i5-670 3.46GHz
      for (uint32_t n = 5; n <= stop; n += 2) {
        uint32_t s = U32SQRT(n);
        uint32_t i = 0;
        for (; primes16Bit[i] <= s && (n % primes16Bit[i]) != 0; i++)
          ;
        if (primes16Bit[i] > s) {
          if (primes16Bit[i] <= keep)
            primes16Bit.push_back(n);
          if (n > resetSieve.getEliminateUpTo())
            // generate the prime numbers up to n^2 and call
            // primeNumberFinder.sieve(p) for each generated prime
            primeNumberGenerator.sieve(n);
        }
      }
      primeNumberGenerator.finish();
    }
    primeNumberFinder.finish();
    for (uint32_t i = 0; i < COUNTS_SIZE; i++)
      counts_[i] += primeNumberFinder.getCounts(i);
  }

  // set status_ to 100.0 (percent)
  parent_->doStatus(10);
  timeElapsed_ = (static_cast<double> (std::clock()) - timeElapsed_) /
      static_cast<double> (CLOCKS_PER_SEC);
}
