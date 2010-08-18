//
// OpenSSLInitializer.cpp
//
// $Id: //poco/1.3/Crypto/src/OpenSSLInitializer.cpp#7 $
//
// Library: Crypto
// Package: CryotpCore
// Module:  OpenSSLInitializer
//
// Copyright (c) 2006-2009, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/Crypto/OpenSSLInitializer.h"
#include "Poco/RandomStream.h"
#include "Poco/Thread.h"
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#if SSLEAY_VERSION_NUMBER >= 0x0907000L
#include <openssl/conf.h>
#endif


using Poco::RandomInputStream;
using Poco::Thread;


namespace Poco {
namespace Crypto {


Poco::FastMutex* OpenSSLInitializer::_mutexes(0);
Poco::FastMutex OpenSSLInitializer::_mutex;
int OpenSSLInitializer::_rc(0);


static OpenSSLInitializer initializer;


OpenSSLInitializer::OpenSSLInitializer()
{
	initialize();
}


OpenSSLInitializer::~OpenSSLInitializer()
{
	uninitialize();
}


void OpenSSLInitializer::initialize()
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	
	if (++_rc == 1)
	{
#if OPENSSL_VERSION_NUMBER >= 0x0907000L
		OPENSSL_config(NULL);
#endif
		SSL_library_init();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
		
		char seed[SEEDSIZE];
		RandomInputStream rnd;
		rnd.read(seed, sizeof(seed));
		RAND_seed(seed, SEEDSIZE);
		
		int nMutexes = CRYPTO_num_locks();
		_mutexes = new Poco::FastMutex[nMutexes];
		CRYPTO_set_locking_callback(&OpenSSLInitializer::lock);
#ifndef POCO_OS_FAMILY_WINDOWS // SF# 1828231: random unhandled exceptions when linking with ssl
		CRYPTO_set_id_callback(&OpenSSLInitializer::id);
#endif
		CRYPTO_set_dynlock_create_callback(&OpenSSLInitializer::dynlockCreate);
		CRYPTO_set_dynlock_lock_callback(&OpenSSLInitializer::dynlock);
		CRYPTO_set_dynlock_destroy_callback(&OpenSSLInitializer::dynlockDestroy);
	}
}


void OpenSSLInitializer::uninitialize()
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (--_rc == 0)
	{
		EVP_cleanup();
		ERR_free_strings();
		CRYPTO_set_locking_callback(0);
		delete [] _mutexes;
	}
}


void OpenSSLInitializer::lock(int mode, int n, const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
		_mutexes[n].lock();
	else
		_mutexes[n].unlock();
}


unsigned long OpenSSLInitializer::id()
{
	// Note: we use an old-style C cast here because
	// neither static_cast<> nor reinterpret_cast<>
	// work uniformly across all platforms.
	return (unsigned long) Poco::Thread::currentTid();
}


struct CRYPTO_dynlock_value* OpenSSLInitializer::dynlockCreate(const char* file, int line)
{
	return new CRYPTO_dynlock_value;
}


void OpenSSLInitializer::dynlock(int mode, struct CRYPTO_dynlock_value* lock, const char* file, int line)
{
	poco_check_ptr (lock);

	if (mode & CRYPTO_LOCK)
		lock->_mutex.lock();
	else
		lock->_mutex.unlock();
}


void OpenSSLInitializer::dynlockDestroy(struct CRYPTO_dynlock_value* lock, const char* file, int line)
{
	delete lock;
}


} } // namespace Poco::Crypto
