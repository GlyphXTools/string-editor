#include "document.h"
#include "exceptions.h"
using namespace std;

static const uint8_t VDF_VERSION = 0x01;

#pragma pack(1)
struct FILEINFO
{
	uint32_t nVersions;
	uint32_t nPostfixes;
	uint16_t language;
	uint8_t  type;
};

struct VERSIONDESC
{
	uint64_t saved;
	uint32_t lenAuthor;
	uint32_t lenNotes;
	uint32_t maxString;
	uint32_t nStrings;
	uint32_t nLanguages;
};

struct LANGUAGEDESC
{
	uint32_t nStrings;
	uint16_t language;
};

struct STRINGDESC
{
	uint32_t id;
	uint32_t position;
	uint32_t name;
	uint32_t comment;
	uint8_t  flags;
};

struct VALUEDESC
{
	uint32_t id;
	uint32_t offset;
};

struct POSTFIXINFO
{
	uint16_t language;
	uint32_t length;
};
#pragma pack()

static void WriteString(IFile& output, const wstring& str)
{
	unsigned long size = (unsigned long)((str.length() + 1) * sizeof(wstring::value_type));
	if (output.write(str.c_str(), size) != size)
	{
		throw WriteException();
	}
}

static wstring ReadString(IFile& input, unsigned long len)
{
	wstring::value_type* buf = new wstring::value_type[len];
	unsigned long size = (unsigned long)(len * sizeof(wstring::value_type));
	if (input.read(buf, size) != size)
	{
		delete[] buf;
		throw ReadException();
	}
	wstring str = buf;
	delete[] buf;
	return str;
}

void Document::write(IFile& output) const
{
	vector<size_t> starts;

	// Write file signature
	uint8_t signature[4] = {'V','D','F', VDF_VERSION};
	if (output.write(signature, 4) != 4)
	{
		throw WriteException();
	}

	// Write counts
	FILEINFO info;
	info.nVersions  = htolel((unsigned long)m_versions.size());
	info.nPostfixes = htolel((unsigned long)m_newPostfixes.size());
	info.language   = htoles(m_curLanguage);
	info.type       = (uint8_t)m_type;
	if (output.write(&info, sizeof info) != sizeof info)
	{
		throw WriteException();
	}

	m_buffer.write(output);

	// Write postfixes
	for (map<LANGID,wstring>::const_iterator p = m_newPostfixes.begin(); p != m_newPostfixes.end(); p++)
	{
		POSTFIXINFO info;
		info.language = htolel(p->first);
		info.length   = htolel((unsigned long)p->second.length() + 1);
		if (output.write(&info, sizeof info) != sizeof info)
		{
			throw WriteException();
		}
		WriteString(output, p->second);
	}

	// Write versions
	for (vector<Version>::const_iterator v = m_versions.begin(); v != m_versions.end(); v++)
	{
		VERSIONDESC version;
		version.saved      = htolell(v->m_saved);
		version.lenAuthor  = htolel((unsigned long)v->m_author.length() + 1);
		version.lenNotes   = htolel((unsigned long)v->m_notes.length() + 1);
		version.maxString  = htolel((unsigned long)v->m_strings.size());
		version.nStrings   = htolel((unsigned long)v->diff_strings.size());
		version.nLanguages = htolel((unsigned long)v->m_values.size());

		if (output.write(&version, sizeof version) != sizeof version)
		{
			throw WriteException();
		}

		WriteString(output, v->m_author);
		WriteString(output, v->m_notes);

		// Write changed string infos
		for (set<size_t>::const_iterator p = v->diff_strings.begin(); p != v->diff_strings.end(); p++)
		{
			const StringInfo& str = v->m_strings[*p];

			STRINGDESC desc;
			desc.id       = htolel((uint32_t)*p);
			desc.position = htolel(str.m_position);
			desc.name     = htolel(m_buffer.getStringOffset(str.m_name));
			desc.comment  = htolel(m_buffer.getStringOffset(str.m_comment));
			desc.flags    = htolel(str.m_flags & SF_SAVE_MASK);
			if (output.write(&desc, sizeof desc) != sizeof desc)
			{
				throw WriteException();
			}
		}

		// Writes language
		for (map<LANGID, StringValues>::const_iterator p = v->m_values.begin(); p != v->m_values.end(); p++)
		{
			uint16_t leLang = htoles(p->first);
			if (output.write(&leLang, sizeof leLang) != sizeof leLang)
			{
				throw WriteException();
			}
		}
	}

	// Write changed values, per language, per version
	for (vector<Version>::const_iterator v = m_versions.begin(); v != m_versions.end(); v++)
	{
		for (map<LANGID, StringValues>::const_iterator p = v->m_values.begin(); p != v->m_values.end(); p++)
		{
			const StringValues& values = p->second;

			uint32_t leNumValues = htolel((unsigned long)values.m_changed.size());
			if (output.write(&leNumValues, sizeof leNumValues) != sizeof leNumValues)
			{
				throw WriteException();
			}

			for (set<size_t>::const_iterator q = values.m_changed.begin(); q != values.m_changed.end(); q++)
			{
				VALUEDESC desc;
				desc.id     = htolel((uint32_t)*q);
				desc.offset = htolel((uint32_t)m_buffer.getStringOffset(values.m_virt[*q]));

				if (output.write(&desc, sizeof(VALUEDESC)) != sizeof(VALUEDESC))
				{
					throw WriteException();
				}
			}
		}
	}
}

Document::Document(IFile& input)
{
	try
	{
		uint8_t signature[4];
		if (input.read(signature, 4) != 4)
		{
			throw ReadException();
		}

		uint8_t version = signature[3];
		if (signature[0] != 'V' || signature[1] != 'D' || signature[2] != 'F' || version == 0)
		{
			throw BadFileException();
		}

		// Currently, we only support Version 1 of the VDF file format
		if (version != VDF_VERSION)
		{
			throw UnsupportedVersionException();
		}

		// Read file info
		FILEINFO info;
		if (input.read(&info, sizeof info) != sizeof info)
		{
			throw ReadException();
		}

		unsigned long nVersions  = letohl(info.nVersions);
		unsigned long nPostfixes = letohl(info.nPostfixes);
		m_curLanguage            = letohs(info.language);
		m_type                   = (Type)info.type;

		m_versions.resize(nVersions + 1);

		// Read string data
		m_buffer.read(input);

		// Read postfixes
		for (unsigned long i = 0; i < nPostfixes; i++)
		{
			POSTFIXINFO info;
			if (input.read(&info, sizeof info) != sizeof info)
			{
				throw ReadException();
			}
			wstring str = ReadString(input, letohl(info.length));
			m_oldPostfixes.insert(make_pair(letohl(info.language), str));
		}
		m_newPostfixes = m_oldPostfixes;

		// Read versions
		for (size_t v = 0; v < nVersions; v++)
		{
			VERSIONDESC desc;
			if (input.read(&desc, sizeof desc) != sizeof desc)
			{
				throw ReadException();
			}

			Version& version = m_versions[v];
			version.m_saved  = letohll(desc.saved);
			version.m_author = ReadString(input, letohl(desc.lenAuthor));
			version.m_notes  = ReadString(input, letohl(desc.lenNotes));

			unsigned long nLanguages = letohl(desc.nLanguages);
			unsigned long nStrings   = letohl(desc.nStrings);
			unsigned long maxString  = letohl(desc.maxString);

			// Read changed strings
			version.m_strings.resize(maxString);
			for (unsigned long i = 0; i < nStrings; i++)
			{
				STRINGDESC desc;
				if (input.read(&desc, sizeof desc) != sizeof desc)
				{
					throw ReadException();
				}

				unsigned long id = letohl(desc.id);
				version.diff_strings.insert(id);

				// Set for current version
				StringInfo& str = version.m_strings[ id ];
				str.m_position = letohl(desc.position);
				str.m_flags    = letohl(desc.flags);
				str.m_name     = m_buffer.getString(letohl(desc.name));
				str.m_comment  = m_buffer.getString(letohl(desc.comment));
				str.m_modified = version.m_saved;
			}

			// Read languages
			for (unsigned long i = 0; i < nLanguages; i++)
			{
				uint16_t leLang;
				if (input.read(&leLang, sizeof leLang) != sizeof leLang)
				{
					throw ReadException();
				}
				version.m_values[ letohs(leLang) ];
			}

			version.m_numDifferences = (unsigned long)version.diff_strings.size();
			version.m_numLanguages	 = (unsigned long)version.m_values.size();
			version.m_numStrings     = 0;

			// Copy this version's strings to next version and clear flags
			m_versions[v+1].m_strings = version.m_strings;
			for (size_t i = 0; i < m_versions[v+1].m_strings.size(); i++)
			{
				m_versions[v+1].m_strings[i].m_flags = 0;
				if (m_versions[v+1].m_strings[i].m_name != NULL)
				{
					version.m_numStrings++;
				}
			}
		}

		// Copy the languages from the last version to the current version
		m_versions[nVersions].m_values = m_versions[nVersions - 1].m_values;

		// Read values, per language, per version
		for (size_t v = 0; v < nVersions; v++)
		{
			Version& version = m_versions[v];
			for (map<LANGID, StringValues>::iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
			{
				uint32_t leNumValues;
				if (input.read(&leNumValues, sizeof leNumValues) != sizeof leNumValues)
				{
					throw ReadException();
				}
				unsigned long nValues = letohl(leNumValues);

				// Read changed values
				StringValues& values = p->second;
				values.m_virt.resize( version.m_strings.size() );
				for (unsigned long j = 0; j < nValues; j++)
				{
					VALUEDESC desc;
					if (input.read(&desc, sizeof(VALUEDESC)) != sizeof(VALUEDESC))
					{
						throw ReadException();
					}

					unsigned long id  = letohl(desc.id);
					values.m_virt[id] = m_buffer.getString(letohl(desc.offset));
					values.m_changed.insert(id);
				}
				version.m_numDifferences += (unsigned long)values.m_changed.size();

				// Copy values to next version, if it also has the language
				map<LANGID, StringValues>::iterator q = m_versions[v+1].m_values.find(p->first);
				if (q != m_versions[v+1].m_values.end())
				{
					q->second.m_virt = values.m_virt;
				}
			}
		}

		// Set cached values
		m_curVersion  = &m_versions.back();
		m_curValues   = &m_curVersion->m_values[m_curLanguage];

		// Set names set and create freelist
		m_strings.resize( m_curVersion->m_strings.size() );
		for (size_t i = 0; i < m_curVersion->m_strings.size(); i++)
		{
			if (m_curVersion->m_strings[i].m_name != NULL)
			{
				m_strings[i].m_name    = m_curVersion->m_strings[i].m_name;
				m_strings[i].m_comment = m_curVersion->m_strings[i].m_comment;
				m_curVersion->m_strings[i].m_name    = m_strings[i].m_name.c_str();
				m_curVersion->m_strings[i].m_comment = m_strings[i].m_comment.c_str();
				
				m_names.insert(make_pair(m_strings[i].m_name, (unsigned int)i));
			}
			else
			{
				m_freelist.push(i);
			}
		}

		// Allocate space for physical strings in last (current) version
		for (map<LANGID, StringValues>::iterator p = m_curVersion->m_values.begin(); p != m_curVersion->m_values.end(); p++)
		{
			p->second.m_phys.resize( p->second.m_virt.size() );
			for (size_t i = 0; i < p->second.m_virt.size(); i++)
			{
				if (m_curVersion->m_strings[i].m_name != NULL)
				{
					p->second.m_phys[i] = p->second.m_virt[i];
					p->second.m_virt[i] = p->second.m_phys[i].c_str();
				}
			}
		}
	}
	catch (...)
	{
		m_buffer.clear();
		throw;
	}
}
