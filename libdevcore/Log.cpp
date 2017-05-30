/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Log.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Log.h"

#include <thread>
#ifdef __APPLE__
#include <pthread.h>
#endif
#include "Guards.h"
using namespace std;
using namespace dev;

//⊳⊲◀▶■▣▢□▷◁▧▨▩▲◆◉◈◇◎●◍◌○◼☑☒☎☢☣☰☀♽♥♠✩✭❓✔✓✖✕✘✓✔✅⚒⚡⦸⬌∅⁕«««»»»⚙

// Logging
int dev::g_logVerbosity = 5;
mutex x_logOverride;

/// Map of Log Channel types to bool, false forces the channel to be disabled, true forces it to be enabled.
/// If a channel has no entry, then it will output as long as its verbosity (LogChannel::verbosity) is less than
/// or equal to the currently output verbosity (g_logVerbosity).
static map<type_info const*, bool> s_logOverride;

#ifdef _WIN32
const char* LogChannel::name() { return EthGray "..."; }
const char* LeftChannel::name() { return EthNavy "<--"; }
const char* RightChannel::name() { return EthGreen "-->"; }
const char* WarnChannel::name() { return EthOnRed EthBlackBold "  X"; }
const char* NoteChannel::name() { return EthBlue "  i"; }
const char* DebugChannel::name() { return EthWhite "  D"; }
#else
const char* LogChannel::name() { return EthGray "···"; }
const char* LeftChannel::name() { return EthNavy "◀▬▬"; }
const char* RightChannel::name() { return EthGreen "▬▬▶"; }
const char* WarnChannel::name() { return EthOnRed EthBlackBold "  ✘"; }
const char* NoteChannel::name() { return EthBlue "  ℹ"; }
const char* DebugChannel::name() { return EthWhite "  ◇"; }
#endif

LogOutputStreamBase::LogOutputStreamBase(char const* _id, std::type_info const* _info, unsigned _v, bool _autospacing):
	m_autospacing(_autospacing),
	m_verbosity(_v)
{
	Guard l(x_logOverride);
	auto it = s_logOverride.find(_info);
	if ((it != s_logOverride.end() && it->second) || (it == s_logOverride.end() && (int)_v <= g_logVerbosity))
	{
		time_t rawTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		char buf[24];
		if (strftime(buf, 24, "%X", localtime(&rawTime)) == 0)
			buf[0] = '\0'; // empty if case strftime fails
		static char const* c_begin = "  " EthViolet;
		static char const* c_sep1 = EthReset EthBlack "|" EthNavy;
		static char const* c_sep2 = EthReset EthBlack "|" EthTeal;
		static char const* c_end = EthReset "  ";
		m_sstr << _id << c_begin << buf << c_sep1 << getThreadName() << ThreadContext::join(c_sep2) << c_end;
	}
}

/// Associate a name with each thread for nice logging.
struct ThreadLocalLogName
{
	ThreadLocalLogName(char const* _name) { name = _name; }
	thread_local static char const* name;
};

thread_local char const* ThreadLocalLogName::name;

thread_local static std::vector<std::string> logContexts;

/// Associate a name with each thread for nice logging.
struct ThreadLocalLogContext
{
	ThreadLocalLogContext() = default;

	void push(std::string const& _name)
	{
		logContexts.push_back(_name);
	}

	void pop()
	{
		logContexts.pop_back();
	}

	string join(string const& _prior)
	{
		string ret;
		for (auto const& i: logContexts)
			ret += _prior + i;
		return ret;
	}
};

ThreadLocalLogContext g_logThreadContext;

ThreadLocalLogName g_logThreadName("main");

void dev::ThreadContext::push(string const& _n)
{
	g_logThreadContext.push(_n);
}

void dev::ThreadContext::pop()
{
	g_logThreadContext.pop();
}

string dev::ThreadContext::join(string const& _prior)
{
	return g_logThreadContext.join(_prior);
}

// foward declare without all of Windows.h
#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#endif

string dev::getThreadName()
{
#if defined(__linux__) || defined(__APPLE__)
	char buffer[128];
	pthread_getname_np(pthread_self(), buffer, 127);
	buffer[127] = 0;
	return buffer;
#else
	return ThreadLocalLogName::name ? ThreadLocalLogName::name : "<unknown>";
#endif
}

void dev::setThreadName(char const* _n)
{
#if defined(__linux__)
	pthread_setname_np(pthread_self(), _n);
#elif defined(__APPLE__)
	pthread_setname_np(_n);
#else
	ThreadLocalLogName::name = _n;
#endif
}

void dev::simpleDebugOut(std::string const& _s, char const*)
{
	static SpinLock s_lock;
	SpinGuard l(s_lock);

	cerr << _s << endl << flush;

	// helpful to use OutputDebugString on windows
	#ifdef _WIN32
	{
		OutputDebugStringA(_s.data());
		OutputDebugStringA("\n");
	}
	#endif
}

std::function<void(std::string const&, char const*)> dev::g_logPost = simpleDebugOut;
