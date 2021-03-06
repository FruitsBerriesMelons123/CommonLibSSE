#include "REL/Relocation.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <exception>
#include <ios>
#include <sstream>
#include <system_error>

#include "RE/RTTI.h"

#include "SKSE/Logger.h"


namespace REL
{
	namespace Impl
	{
		void kmp_table(const Array<std::uint8_t>& W, Array<std::size_t>& T)
		{
			std::size_t pos = 1;
			std::size_t cnd = 0;

			T[0] = NPOS;

			while (pos < W.size()) {
				if (W[pos] == W[cnd]) {
					T[pos] = T[cnd];
				} else {
					T[pos] = cnd;
					cnd = T[cnd];
					while (cnd != NPOS && W[pos] != W[cnd]) {
						cnd = T[cnd];
					}
				}
				++pos;
				++cnd;
			}

			T[pos] = cnd;
		}


		void kmp_table(const Array<std::uint8_t>& W, const Array<bool>& M, Array<std::size_t>& T)
		{
			std::size_t pos = 1;
			std::size_t cnd = 0;

			T[0] = NPOS;

			while (pos < W.size()) {
				if (!M[pos] || !M[cnd] || W[pos] == W[cnd]) {
					T[pos] = T[cnd];
				} else {
					T[pos] = cnd;
					cnd = T[cnd];
					while (cnd != NPOS && M[pos] && M[cnd] && W[pos] != W[cnd]) {
						cnd = T[cnd];
					}
				}
				++pos;
				++cnd;
			}

			T[pos] = cnd;
		}


		std::size_t kmp_search(const Array<std::uint8_t>& S, const Array<std::uint8_t>& W)
		{
			std::size_t j = 0;
			std::size_t k = 0;
			Array<std::size_t> T(W.size() + 1);
			kmp_table(W, T);

			while (j < S.size()) {
				if (W[k] == S[j]) {
					++j;
					++k;
					if (k == W.size()) {
						return j - k;
					}
				} else {
					k = T[k];
					if (k == NPOS) {
						++j;
						++k;
					}
				}
			}

			return 0xDEADBEEF;
		}


		std::size_t kmp_search(const Array<std::uint8_t>& S, const Array<std::uint8_t>& W, const Array<bool>& M)
		{
			std::size_t j = 0;
			std::size_t k = 0;
			Array<std::size_t> T(W.size() + 1);
			kmp_table(W, M, T);

			while (j < S.size()) {
				if (!M[k] || W[k] == S[j]) {
					++j;
					++k;
					if (k == W.size()) {
						return j - k;
					}
				} else {
					k = T[k];
					if (k == NPOS) {
						++j;
						++k;
					}
				}
			}

			return 0xDEADBEEF;
		}
	}


	std::uint32_t Module::Section::RVA() const
	{
		assert(rva != 0xDEADBEEF);
		return rva;
	}


	std::uintptr_t Module::Section::BaseAddr() const
	{
		assert(addr != 0xDEADBEEF);
		return addr;
	}


	std::size_t Module::Section::Size() const
	{
		assert(size != 0xDEADBEEF);
		return size;
	}


	std::uintptr_t Module::BaseAddr()
	{
		return _info.base;
	}


	std::size_t Module::Size()
	{
		return _info.size;
	}


	auto Module::GetSection(ID a_id)
		-> Section
	{
		assert(a_id < ID::kTotal);
		return _info.sections.arr[a_id].section;
	}


	auto Module::GetVersion() noexcept
		-> SKSE::Version
	{
		return _info.version;
	}


	Module::ModuleInfo::ModuleInfo() :
		handle(GetModuleHandle(0)),
		base(0),
		sections(),
		size(0),
		version()
	{
		base = reinterpret_cast<std::uintptr_t>(handle);

		auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		auto ntHeader = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const std::uint8_t*>(dosHeader) + dosHeader->e_lfanew);
		size = ntHeader->OptionalHeader.SizeOfCode;

		const auto sectionHeader = IMAGE_FIRST_SECTION(ntHeader);
		for (std::size_t i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i) {
			const auto& section = sectionHeader[i];
			for (auto& elem : sections.arr) {
				auto length = std::min<std::size_t>(elem.name.length(), IMAGE_SIZEOF_SHORT_NAME);
				if (std::memcmp(elem.name.data(), section.Name, length) == 0 && (section.Characteristics & elem.flags) == elem.flags) {
					elem.section.rva = section.VirtualAddress;
					elem.section.addr = base + section.VirtualAddress;
					elem.section.size = section.Misc.VirtualSize;
					break;
				}
			}
		}

		BuildVersionInfo();
	}


	void Module::ModuleInfo::BuildVersionInfo()
	{
		char fileName[MAX_PATH];
		if (!GetModuleFileNameA(handle, fileName, MAX_PATH)) {
			assert(false);
			return;
		}

		DWORD dummy;
		std::vector<char> buf(GetFileVersionInfoSizeA(fileName, &dummy));
		if (buf.size() == 0) {
			assert(false);
			return;
		}

		if (!GetFileVersionInfoA(fileName, 0, buf.size(), buf.data())) {
			assert(false);
			return;
		}

		LPVOID verBuf;
		UINT verLen;
		if (!VerQueryValueA(buf.data(), "\\StringFileInfo\\040904B0\\ProductVersion", &verBuf, &verLen)) {
			assert(false);
			return;
		}

		std::istringstream ss(static_cast<const char*>(verBuf));
		std::string token;
		for (std::size_t i = 0; i < 4 && std::getline(ss, token, '.'); ++i) {
			version[i] = static_cast<std::uint16_t>(std::stoi(token));
		}
	}


	decltype(Module::_info) Module::_info;


	bool IDDatabase::Init()
	{
		return _db.Load();
	}


#ifdef _DEBUG
	std::uint64_t IDDatabase::OffsetToID(std::uint64_t a_address)
	{
		return _db.OffsetToID(a_address);
	}
#endif


	std::uint64_t IDDatabase::IDToOffset(std::uint64_t a_id)
	{
		return _db.IDToOffset(a_id);
	}


	bool IDDatabase::IDDatabaseImpl::Load()
	{
		auto version = Module::GetVersion();
		return Load(std::move(version));
	}


	bool IDDatabase::IDDatabaseImpl::Load(std::uint16_t a_major, std::uint16_t a_minor, std::uint16_t a_revision, std::uint16_t a_build)
	{
		SKSE::Version version(a_major, a_minor, a_revision, a_build);
		return Load(std::move(version));
	}


	bool IDDatabase::IDDatabaseImpl::Load(SKSE::Version a_version)
	{
		std::string fileName = "Data/SKSE/Plugins/version-";
		fileName += std::to_string(a_version[0]);
		fileName += '-';
		fileName += std::to_string(a_version[1]);
		fileName += '-';
		fileName += std::to_string(a_version[2]);
		fileName += '-';
		fileName += std::to_string(a_version[3]);
		fileName += ".bin";

		std::error_code ec;
		std::filesystem::path path(fileName);
		if (!std::filesystem::exists(path, ec) || ec) {
			assert(false);
			return false;
		}

		std::ifstream file(path, std::ios_base::in | std::ios_base::binary);
		file.exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
		if (!file.is_open()) {
			assert(false);
			return false;
		}

		try {
			IStream input(file);
			return DoLoad(input);
		} catch (std::exception& e) {
			_ERROR("%s", e.what());
			assert(false);
			return false;
		}
	}


#ifdef _DEBUG
	std::uint64_t IDDatabase::IDDatabaseImpl::OffsetToID(std::uint64_t a_address)
	{
		auto it = _ids.find(a_address);
		if (it != _ids.end()) {
			return it->second;
		} else {
			throw std::runtime_error("Failed to find address in ID database");
		}
	}
#endif


	std::uint64_t IDDatabase::IDDatabaseImpl::IDToOffset(std::uint64_t a_id)
	{
		return _offsets[a_id];
	}


	void IDDatabase::IDDatabaseImpl::Header::Read(IStream& a_input)
	{
		_part1.Read(a_input);
		_exeName.resize(_part1.nameLen);
		a_input->read(_exeName.data(), _exeName.size());
		_part2.Read(a_input);
	}


	void IDDatabase::IDDatabaseImpl::Header::Part1::Read(IStream& a_input)
	{
		a_input->read(reinterpret_cast<char*>(this), sizeof(Part1));
		if (format != 1) {
			throw std::runtime_error("Unexpected format");
		}
	}


	void IDDatabase::IDDatabaseImpl::Header::Part2::Read(IStream& a_input)
	{
		a_input->read(reinterpret_cast<char*>(this), sizeof(Part2));
	}


	bool IDDatabase::IDDatabaseImpl::DoLoad(IStream& a_input)
	{
		Header header;
		header.Read(a_input);

		if (Module::GetVersion() != header.GetVersion()) {
			assert(false);
			return false;
		}

		_offsets.reserve(header.AddrCount());
#ifdef _DEBUG
		_ids.reserve(header.AddrCount());
#endif

		bool success = true;
		try {
			DoLoadImpl(a_input, header);
		} catch ([[maybe_unused]] std::exception& e) {
			assert(false);
			_ERROR("%s in ID database", e.what());
			success = false;
		}

		_offsets.shrink_to_fit();
		return success;
	}


	void IDDatabase::IDDatabaseImpl::DoLoadImpl(IStream& a_input, Header& a_header)
	{
		std::uint8_t type;
		std::uint64_t id;
		std::uint64_t offset;
		std::uint64_t prevID = 0;
		std::uint64_t prevOffset = 0;
		for (std::size_t i = 0; i < a_header.AddrCount(); ++i) {
			a_input.readin(type);
			std::uint8_t lo = type & 0xF;
			std::uint8_t hi = type >> 4;

			switch (lo) {
			case 0:
				a_input.readin(id);
				break;
			case 1:
				id = prevID + 1;
				break;
			case 2:
				id = prevID + a_input.readout<std::uint8_t>();
				break;
			case 3:
				id = prevID - a_input.readout<std::uint8_t>();
				break;
			case 4:
				id = prevID + a_input.readout<std::uint16_t>();
				break;
			case 5:
				id = prevID - a_input.readout<std::uint16_t>();
				break;
			case 6:
				id = a_input.readout<std::uint16_t>();
				break;
			case 7:
				id = a_input.readout<std::uint32_t>();
				break;
			default:
				throw std::runtime_error("Unhandled type");
			}

			std::uint64_t tmp = (hi & 8) != 0 ? (prevOffset / a_header.PSize()) : prevOffset;

			switch (hi & 7) {
			case 0:
				a_input.readin(offset);
				break;
			case 1:
				offset = tmp + 1;
				break;
			case 2:
				offset = tmp + a_input.readout<std::uint8_t>();
				break;
			case 3:
				offset = tmp - a_input.readout<std::uint8_t>();
				break;
			case 4:
				offset = tmp + a_input.readout<std::uint16_t>();
				break;
			case 5:
				offset = tmp - a_input.readout<std::uint16_t>();
				break;
			case 6:
				offset = a_input.readout<std::uint16_t>();
				break;
			case 7:
				offset = a_input.readout<std::uint32_t>();
				break;
			default:
				throw std::runtime_error("Unhandled type");
			}

			if ((hi & 8) != 0) {
				offset *= a_header.PSize();
			}

			if (id >= _offsets.size()) {
				_offsets.resize(id + 1, 0);
			}
			_offsets[id] = offset;
#ifdef _DEBUG
			_ids[offset] = id;
#endif

			prevOffset = offset;
			prevID = id;
		}
	}


	decltype(IDDatabase::_db) IDDatabase::_db;


	std::uint64_t ID::operator*() const
	{
		return GetOffset();
	}

	std::uint64_t ID::GetOffset() const
	{
		return IDDatabase::IDToOffset(_id);
	}


	VTable::VTable(const char* a_name, std::uint32_t a_offset) :
		_address(0xDEADBEEF)
	{
		auto typeDesc = LocateTypeDescriptor(a_name);
		if (!typeDesc) {
			assert(false);
			return;
		}

		auto col = LocateCOL(typeDesc, a_offset);
		if (!col) {
			assert(false);
			return;
		}

		auto vtbl = LocateVtbl(col);
		if (!vtbl) {
			assert(false);
			return;
		}

		_address = reinterpret_cast<std::uintptr_t>(vtbl);
	}


	void* VTable::GetPtr() const
	{
		return reinterpret_cast<void*>(GetAddress());
	}


	std::uintptr_t VTable::GetAddress() const
	{
		assert(_address != 0xDEADBEEF);
		return _address;
	}


	std::uintptr_t VTable::GetOffset() const
	{
		return GetAddress() - Module::BaseAddr();
	}


	RE::RTTI::TypeDescriptor* VTable::LocateTypeDescriptor(const char* a_name) const
	{
		auto name = const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(a_name));
		auto section = Module::GetSection(ID::kData);
		auto start = section.BasePtr<std::uint8_t>();

		Impl::Array<std::uint8_t> haystack(start, section.Size());
		Impl::Array<std::uint8_t> needle(name, std::strlen(a_name));
		auto addr = start + Impl::kmp_search(haystack, needle);
		addr -= offsetof(RE::RTTI::TypeDescriptor, name);

		return reinterpret_cast<RE::RTTI::TypeDescriptor*>(addr);
	}


	RE::RTTI::CompleteObjectLocator* VTable::LocateCOL(RE::RTTI::TypeDescriptor* a_typeDesc, std::uint32_t a_offset) const
	{
		auto typeDesc = reinterpret_cast<std::uintptr_t>(a_typeDesc);
		auto rva = static_cast<std::uint32_t>(typeDesc - Module::BaseAddr());

		auto section = Module::GetSection(ID::kRData);
		auto base = section.BasePtr<std::uint8_t>();
		auto start = reinterpret_cast<std::uint32_t*>(base);
		auto end = reinterpret_cast<std::uint32_t*>(base + section.Size());

		for (auto iter = start; iter < end; ++iter) {
			if (*iter == rva) {
				// both base class desc and col can point to the type desc so we check
				// the next int to see if it can be an rva to decide which type it is
				if (iter[1] < section.RVA()) {
					continue;
				}

				auto ptr = reinterpret_cast<std::uint8_t*>(iter);
				auto col = reinterpret_cast<RE::RTTI::CompleteObjectLocator*>(ptr - offsetof(RE::RTTI::CompleteObjectLocator, typeDescriptor));
				if (col->offset != a_offset) {
					continue;
				}

				return col;
			}
		}

		return 0;
	}


	void* VTable::LocateVtbl(RE::RTTI::CompleteObjectLocator* a_col) const
	{
		auto col = reinterpret_cast<std::uintptr_t>(a_col);

		auto section = Module::GetSection(ID::kRData);
		auto base = section.BasePtr<std::uint8_t>();
		auto start = reinterpret_cast<std::uintptr_t*>(base);
		auto end = reinterpret_cast<std::uintptr_t*>(base + section.Size());

		for (auto iter = start; iter < end; ++iter) {
			if (*iter == col) {
				return iter + 1;
			}
		}

		return 0;
	}
}
