#include <algorithm>
#include "document.h"
#include "exceptions.h"
using namespace std;

void Document::setPostfix(LANGID language, const wstring& postfix )
{
	m_newPostfixes[language] = postfix;
}

const vector<const wchar_t*>& Document::getValues(LANGID language) const
{
	return m_curVersion->m_values.find(language)->second.m_virt;
}

void Document::getLanguages(std::set<LANGID>& languages, int version) const
{
	const Version* pVersion = (version == -1) ? m_curVersion : &m_versions[version];

	for (map<LANGID, StringValues>::const_iterator p = pVersion->m_values.begin(); p != pVersion->m_values.end(); p++)
	{
		languages.insert(p->first);
	}
}

void Document::addLanguage(LANGID language)
{
	// Add the language
	Version&      version = m_versions.back();
	StringValues& values  = version.m_values[language];
	
	values.m_virt.resize( version.m_strings.size() );
	values.m_phys.resize( version.m_strings.size() );

	for (size_t i = 0; i < version.m_strings.size(); i++)
	{
		values.m_virt[i] = values.m_phys[i].c_str();
		checkChangedAll((unsigned int)i);
	}

	m_newPostfixes[language];
}

void Document::changeLanguage(LANGID from, LANGID to)
{
	Version& version = m_versions.back();
	map<LANGID, StringValues>::const_iterator p = version.m_values.find(from);
	if (p != version.m_values.end() && version.m_values.find(to) == version.m_values.end())
	{
		StringValues& values = version.m_values[to];
		values.m_phys = p->second.m_phys;
		version.m_values.erase(from);

		if (m_curLanguage == from)
		{
			setActiveLanguage(to);
		}

		values.m_virt.resize( version.m_strings.size() );
		for (size_t i = 0; i < version.m_strings.size(); i++)
		{
			if (version.m_strings[i].m_name != NULL)
			{
				values.m_virt[i] = values.m_phys[i].c_str();
			}
			checkChanged((unsigned int)i);
		}

		m_newPostfixes[to] = m_newPostfixes[from];
		m_newPostfixes.erase(from);
	}
}

void Document::deleteLanguage(LANGID language)
{
	m_versions.back().m_values.erase(language);
}

bool Document::setActiveLanguage(LANGID language)
{
	map<LANGID, StringValues>::iterator p = m_curVersion->m_values.find(language);
	if (p != m_curVersion->m_values.end())
	{
		m_curLanguage = language;
		m_curValues   = &p->second;
		return true;
	}
	return false;
}

void Document::getVersions( std::vector<VersionInfo>& versions ) const
{
	versions.resize(m_versions.size());
	for (size_t i = 0; i < m_versions.size(); i++)
	{
		versions[i] = m_versions[i];
	}
}

const Document::StringInfo& Document::getString(unsigned int id, int version) const
{
	const Version* pVersion = (version < 0) ? m_curVersion : &m_versions[version];
	return pVersion->m_strings[id];
}

const wchar_t* Document::getValue(unsigned int id, int version) const
{
	if (version < 0)
	{
		return m_curValues->m_virt[id];
	}
	
	map<LANGID, StringValues>::const_iterator p = m_versions[version].m_values.find(m_curLanguage);
	return (p == m_versions[version].m_values.end()) ? NULL : p->second.m_virt[id];
}

bool Document::hasStringChanged(unsigned int id, int version) const
{
	const Version* pVersion = (version < 0) ? m_curVersion : &m_versions[version];
	return pVersion->diff_strings.find(id) != pVersion->diff_strings.end();
}

bool Document::hasValueChanged(unsigned int id, int version) const
{
	const StringValues* values = (version < 0) ? m_curValues : &m_versions[version].m_values.find(m_curLanguage)->second;
	return values->m_changed.find(id) != values->m_changed.end();
}

// Check if this string has changed with respect to the previous version and
// adjust m_modified timestamp accordingly.
void Document::checkChanged(unsigned int id)
{
	// Note: m_curVersion and m_curLanguage point to the latest version and language
	// in which the changed string is.
	Version&    newver = *m_curVersion;
	StringInfo& newstr = newver.m_strings[id];

	bool infoChanged  = true;
	bool valueChanged = true;
	newstr.m_modified = DateTime().getEpochSeconds();

	if (m_versions.size() > 1)
	{
		const Version& oldver = m_versions[m_versions.size()-2];
		if (~newstr.m_flags & SF_NEW && id < oldver.m_strings.size())
		{
			// The same string existed in the previous version as well, compare it
			const StringInfo& oldstr = oldver.m_strings[id];

			if (newstr.m_position == oldstr.m_position     &&
				wcscmp(newstr.m_name, oldstr.m_name) == 0  &&
				wcscmp(newstr.m_comment, oldstr.m_comment) == 0)
			{
				// The info hasn't changed
				infoChanged = false;
			}

			map<LANGID, StringValues>::const_iterator p = oldver.m_values.find(m_curLanguage);
			if (p != oldver.m_values.end() && wcscmp(m_curValues->m_virt[id], p->second.m_virt[id]) == 0)
			{
				// The value hasn't changed
				valueChanged = false;
			}

			if (!infoChanged && !valueChanged)
			{
				// It hasn't been changed, so copy the modified date from the previous version
				newstr.m_modified = oldstr.m_modified;
			}
		}
	}

	// Add it to the appropriate lists
	if (infoChanged)  newver.diff_strings.insert(id);
	else              newver.diff_strings.erase(id);
	if (valueChanged) m_curValues->m_changed.insert(id);
	else              m_curValues->m_changed.erase(id);
}

// Check if this string has changed with respect to the previous version and
// adjust m_modified timestamp accordingly. Checks all languages
void Document::checkChangedAll(unsigned int id)
{
	// Note: m_curVersion points to the latest version
	size_t      version = m_versions.size() - 1;
	Version&    newver  = *m_curVersion;
	StringInfo& newstr  = newver.m_strings[id];

	newstr.m_modified = DateTime().getEpochSeconds();

	if (m_versions.size() > 1 && ~newstr.m_flags & SF_NEW && id < m_versions[version-1].m_strings.size())
	{
		// The same string existed in the previous version as well, compare it
		const Version&    oldver = m_versions[version-1];
		const StringInfo& oldstr = oldver.m_strings[id];

		bool changed = false;
		if (newstr.m_position != oldstr.m_position || wcscmp(newstr.m_name, oldstr.m_name) != 0 || wcscmp(newstr.m_comment, oldstr.m_comment) != 0)
		{
			changed = true;
			newver.diff_strings.insert(id);
		}
		else newver.diff_strings.erase(id);

		for (map<LANGID, StringValues>::iterator p = newver.m_values.begin(); p != newver.m_values.end(); p++)
		{
			map<LANGID, StringValues>::const_iterator q = oldver.m_values.find(p->first);
			if (q == oldver.m_values.end() || wcscmp(p->second.m_virt[id], q->second.m_virt[id]) != 0)
			{
				p->second.m_changed.insert(id);
				changed = true;
			}
			else p->second.m_changed.erase(id);
		}

		if (!changed)
		{
			// It hasn't been changed, so copy the modified date from the previous version
			newstr.m_modified = oldstr.m_modified;
		}
	}
	else
	{
		// No previous version, than it has changed by definition
		newver.diff_strings.insert(id);
		for (map<LANGID, StringValues>::iterator p = newver.m_values.begin(); p != newver.m_values.end(); p++)
		{
			p->second.m_changed.insert(id);
		}
	}
}

unsigned int Document::addString()
{
	Version& version = m_versions.back();

	// A litte note about the whole capacity() thing:
	// We cache strings and values in wstring's for the current version.
	// When, by adding a string, we enlarge the vector, the strings could be
	// moved somewhere else, and the wchar_t*'s that point to them would become
	// invalid.
	// We can't avoid this, so we reassign all wchar_t*'s should this occur.
	// To reduce the frequency of this event, we manually increase the capacity
	// in steps of 64 instead a possible vector's brain-dead implementation.

	// Add it to the list
	unsigned int id;
	if (m_freelist.empty())
	{
		// Increase vector size
		id = (unsigned int)version.m_strings.size();
		for (map<LANGID, StringValues>::iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
		{
			size_t oldCapacity = p->second.m_phys.capacity();
			p->second.m_phys.reserve((oldCapacity + 63) & -64);
			p->second.m_phys.push_back(L"");
			if (p->second.m_phys.capacity() != oldCapacity)
			{
				// When the capacity of m_phys changes, we need to reassign all m_virt's,
				// because the pointers have probably changed.
				for (size_t i = 0; i < p->second.m_virt.size(); i++)
				{
					p->second.m_virt[i] = p->second.m_phys[i].c_str();
				}
			}
			p->second.m_virt.push_back(p->second.m_phys[id].c_str());
		}

		size_t oldCapacity = m_strings.capacity();
		m_strings.reserve((oldCapacity + 63) & -64);
		m_strings.push_back(CurrentString());
		if (m_strings.capacity() != oldCapacity)
		{
			// When the capacity of m_strings changes, we need to reassign all version.m_strings's,
			// because the pointers have probably changed
			for (size_t i = 0; i < version.m_strings.size(); i++)
			{
				if (version.m_strings[i].m_name != NULL)
				{
					version.m_strings[i].m_name    = m_strings[i].m_name.c_str();
					version.m_strings[i].m_comment = m_strings[i].m_comment.c_str();
				}
			}
		}
		version.m_strings.push_back(StringInfo());
	}
	else
	{
		// Overwrite vector entry
		id = (unsigned int)m_freelist.top();
		for (map<LANGID, StringValues>::iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
		{
			p->second.m_phys[id] = L"";
			p->second.m_virt[id] = p->second.m_phys[id].c_str();
		}
		m_strings[id] = CurrentString();
	}

	StringInfo si;
	si.m_name     = m_strings[id].m_name.c_str();
	si.m_comment  = m_strings[id].m_comment.c_str();
	si.m_flags    = SF_NEW;
	si.m_modified = DateTime().getEpochSeconds();
	version.m_strings[id] = si;

	m_names.insert(make_pair(m_strings[id].m_name, id));
	checkChangedAll(id);

	// If we got it from the freelist, remove the id
	if (!m_freelist.empty())
	{
		m_freelist.pop();
	}

	return id;
}

bool Document::isValidName(unsigned int id) const
{
	if (m_curVersion == &m_versions.back())
	{
		const wstring& name = m_curVersion->m_strings[id].m_name;

		if (getType() == DT_NAME)
		{
			// Check for duplicates
			multimap<wstring, unsigned int>::const_iterator p = m_names.find(name);
			if (p != m_names.end())
			{
				if (++p != m_names.end() && p->first == name)
				{
					// Duplicate entry
					return false;
				}
			}
		}

		// Check for proper formatting (i.e. identifier characters)
		for (wstring::const_iterator c = name.begin(); c != name.end(); c++)
		{
			if ((*c < L'A' || *c > L'Z') && (*c < L'a' || *c > L'z') && (*c < L'0' || *c > L'9') &&
				*c != L'_' && *c != L'-' && *c != L'.' && *c != L' ')
			{
				return false;
			}
		}

		return !name.empty();
	}
	return true;
}

void Document::setString(unsigned int id, const String& str)
{
	// This is the only place (from outside) direct changes to the current version can be made
	if (m_curVersion == &m_versions.back())
	{
		if (str.m_flags & String::SF_NAME)
		{
			if (m_curVersion->m_strings[id].m_name != NULL)
			{
				// Remove previous value from name set
				m_names.erase( m_names.find(m_curVersion->m_strings[id].m_name) );
			}

			m_strings[id].m_name               = str.m_name; 
			m_curVersion->m_strings[id].m_name = m_strings[id].m_name.c_str();
		}
		
		if (str.m_flags & String::SF_COMMENT)
		{
			m_strings[id].m_comment               = str.m_comment;
			m_curVersion->m_strings[id].m_comment = m_strings[id].m_comment.c_str();
		}

		if (str.m_flags & String::SF_POSITION)
		{
			m_curVersion->m_strings[id].m_position = str.m_position;
		}

		if (str.m_flags & String::SF_VALUE)
		{
			m_curValues->m_phys[id] = str.m_value;
			m_curValues->m_virt[id] = m_curValues->m_phys[id].c_str();
		}

		if (str.m_flags & String::SF_NAME)
		{
			// Add new value to name set
			m_names.insert(make_pair(m_curVersion->m_strings[id].m_name, id));
		}

		checkChanged(id);
	}
}

void Document::deleteString(unsigned int id)
{
	if (m_curVersion == &m_versions.back())
	{
		// Deleted names don't count in collisions, obviously
		m_names.erase( m_names.find( m_curVersion->m_strings[id].m_name) );

		// We don't store the string for the m_changed's as well. By storing the information
		// we know it has been deleted and that's all we need. However, we do need to
		// explicitely remove it from the m_changed lists, so we don't store it accidently.
		m_curVersion->diff_strings.insert(id);
		m_curVersion->m_strings[id].m_name    = NULL;
		m_curVersion->m_strings[id].m_comment = NULL;
		m_curVersion->m_strings[id].m_flags   = 0;
		m_strings[id].m_name.clear();
		m_strings[id].m_comment.clear();
		
		for (map<LANGID, StringValues>::iterator p = m_curVersion->m_values.begin(); p != m_curVersion->m_values.end(); p++)
		{
			p->second.m_phys[id].clear();
			p->second.m_virt[id] = NULL;
			p->second.m_changed.erase(id);
		}

		// This string ID can be reused
		m_freelist.push(id);
	}
}

void Document::setPosition(unsigned int id, unsigned int position)
{
	if (m_curVersion == &m_versions.back() && getType() == DT_INDEX)
	{
		m_curVersion->m_strings[id].m_position = position;
		checkChanged(id);
	}
}

bool Document::setActiveVersion(int version)
{
	if (version == -1)
	{
		version = (int)m_versions.size() - 1;
	}
	m_curVersion = &m_versions[version];

	map<LANGID, StringValues>::iterator p = m_curVersion->m_values.find(m_curLanguage);
	if (p == m_curVersion->m_values.end())
	{
		p = m_curVersion->m_values.begin();
		m_curLanguage = p->first;
		m_curValues = &p->second;
		return false;
	}
	m_curValues = &p->second;
	return true;
}

void Document::saveVersion(const wstring& author, const wstring& notes)
{
	Version& version = m_versions.back();
	
	// Save version information
	version.m_author = author;
	version.m_notes  = notes;
	version.m_saved  = DateTime().getEpochSeconds();
	version.m_numDifferences = (unsigned long)version.diff_strings.size();
	version.m_numLanguages   = (unsigned long)version.m_values.size();
	version.m_numStrings     = 0;

	// Add strings to the string buffer and discard physical copy
	for (size_t i = 0; i != version.m_strings.size(); i++)
	{
		if (version.m_strings[i].m_name != NULL)
		{
			version.m_numStrings++;
			version.m_strings[i].m_name    = m_buffer.addString(m_strings[i].m_name);
			version.m_strings[i].m_comment = m_buffer.addString(m_strings[i].m_comment);
			m_strings[i].m_name.clear();
			m_strings[i].m_comment.clear();
		}
	}

	for (map<LANGID, StringValues>::iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
	{
		for (size_t i = 0; i != version.m_strings.size(); i++)
		{
			if (version.m_strings[i].m_name != NULL)
			{
				p->second.m_virt[i] = m_buffer.addString(p->second.m_phys[i]);
				p->second.m_phys[i].clear();
			}
		}

		version.m_numDifferences += (unsigned long)p->second.m_changed.size();
	}
}

void Document::increaseVersion()
{
	// Copy the last version to new version
	m_versions.push_back( m_versions.back() );
	Version& version = m_versions.back();

	// Clear changes
	version.diff_strings.clear();
	version.m_author.clear();
	version.m_notes.clear();
	for (size_t i = 0; i != version.m_strings.size(); i++)
	{
		version.m_strings[i].m_flags = 0;
	}

	for (map<LANGID, StringValues>::iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
	{
		p->second.m_changed.clear();
	}

	m_oldPostfixes = m_newPostfixes;
}

bool Document::isModified() const
{
	const Version& version = m_versions.back();
	if (version.diff_strings.empty())
	{
		for (map<LANGID,StringValues>::const_iterator p = version.m_values.begin(); p != version.m_values.end(); p++)
		{
			if (!p->second.m_changed.empty())
			{
				return true;
			}
		}
		return false;
	}
	return true;
}

struct LOOKUP
{
	unsigned long m_position;
	size_t        m_index;

	bool operator < (const LOOKUP& o) { 
		return m_position < o.m_position;
	}
};

void Document::exportFile(LANGID language, IFile& output) const
{
	map<LANGID, StringValues>::const_iterator p = m_curVersion->m_values.find(language);
	if (m_curVersion == &m_versions.back() && p != m_curVersion->m_values.end())
	{
		const vector<StringInfo>& strings = m_curVersion->m_strings;
		const StringValues&       values  = p->second;
		
		StringList list;
		list.reserve(strings.size());
		if (getType() == DT_INDEX)
		{
			// We need to add the strings in the order of position, so we create a lookup table
			vector<LOOKUP> lookup(strings.size());
			for (size_t i = 0; i < strings.size(); i++)
			{
				lookup[i].m_position = strings[i].m_position;
				lookup[i].m_index    = i;
			}
			std::sort(lookup.begin(), lookup.end());

			for (size_t i = 0; i < strings.size(); i++)
			{
				size_t idx = lookup[i].m_index;
				list.add(strings[idx].m_name, values.m_virt[idx], strings[idx].m_comment);
			}
		}
		else
		{
			// Just add the strings in any order
			for (size_t i = 0; i < strings.size(); i++)
			{
				if (strings[i].m_name != NULL)
				{
					list.add(strings[i].m_name, values.m_virt[i], strings[i].m_comment);
				}
			}
		}
		list.write(output, getType() == DT_NAME);
	}
}

void Document::addStrings(const StringList& strings, Method method)
{
	if (getType() == DT_INDEX)
	{
		size_t left = 0, right = 0;

		if (method == AM_OVERWRITE)
		{
			vector<LOOKUP> lookups;
			lookups.reserve(m_curVersion->m_strings.size());

			// Create a lookup table of the strings sorted on position
			for (size_t i = 0; i < m_curVersion->m_strings.size(); i++)
			{
				if (m_curVersion->m_strings[i].m_name != NULL)
				{
					LOOKUP lookup;
					lookup.m_index    = i;
					lookup.m_position = m_curVersion->m_strings[i].m_position;
					lookups.push_back(lookup);
				}
			}
			sort(lookups.begin(), lookups.end());

			do
			{
				// While the names are equal, overwrite
				while (left < lookups.size() && right < strings.size() && m_strings[lookups[left].m_index].m_name == strings[right].m_name)
				{
					size_t index = lookups[left].m_index;
					m_curValues->m_phys[index] = strings[right].m_value;
					m_curValues->m_virt[index] = m_curValues->m_phys[index].c_str();
					checkChanged((unsigned int)index);
					left++;
					right++;
				}

				if (right < strings.size())
				{
					// Find the first matching name
					while (left < lookups.size() && m_strings[lookups[left].m_index].m_name != strings[right].m_name)
					{
						left++;
					}
				}

				// Continue overwriting, if possible
			} while (left < lookups.size() && right < strings.size());
		}
		else
		{
			// Find the number of active strings
			for (size_t i = 0; i < m_curVersion->m_strings.size(); i++)
			{
				if (m_curVersion->m_strings[i].m_name != NULL)
				{
					left++;
				}
			}
		}
		
		// Append the rest
		for (; right < strings.size(); left++, right++)
		{
			const wstring& name = strings[right].m_name;

			unsigned int id = addString();
			m_names.insert(make_pair(name, id));

			m_strings[id].m_name                   = name;
			m_curVersion->m_strings[id].m_name     = m_strings[id].m_name.c_str();
			m_curVersion->m_strings[id].m_comment  = m_strings[id].m_comment.c_str();
			m_curVersion->m_strings[id].m_position = (unsigned long)left;

			m_curValues->m_phys[id] = strings[right].m_value;
			m_curValues->m_virt[id] = m_curValues->m_phys[id].c_str();

			checkChanged(id);
		}
	}
	else if (m_curVersion == &m_versions.back() && m_curValues != NULL && method != AM_NONE)
	{
		if (method == AM_UNION || method == AM_UNION_OVERWRITE)
		{
			for (StringList::const_iterator i = strings.begin(); i != strings.end(); i++)
			{
				const wstring& name = i->m_name;
				multimap<wstring, unsigned int>::const_iterator p = m_names.find(name);
				unsigned int id;
				if (p == m_names.end())
				{
					// Name doesn't exist, add it
					id = addString();
					m_names.insert(make_pair(name, id));

					m_strings[id].m_name                  = name;
					m_curVersion->m_strings[id].m_name    = m_strings[id].m_name.c_str();
					m_curVersion->m_strings[id].m_comment = m_strings[id].m_comment.c_str();
				}
				else if (method == AM_UNION_OVERWRITE)
				{
					// Overwrite it
					id = p->second;
				}
				else
				{
					continue;
				}

				m_curValues->m_phys[id] = i->m_value;
				m_curValues->m_virt[id] = m_curValues->m_phys[id].c_str();

				checkChanged(id);
			}
		}
		else if (method == AM_INTERSECT)
		{
			for (size_t i = 0; i < m_curVersion->m_strings.size(); i++)
			{
				const wchar_t* name = m_curVersion->m_strings[i].m_name;
				if (name != NULL && strings.find(name) == strings.end())
				{
					deleteString((unsigned int)i);
				}
			}
		}
		else if (method == AM_DIFFERENCE)
		{
			for (size_t i = 0; i < m_curVersion->m_strings.size(); i++)
			{
				const wchar_t* name = m_curVersion->m_strings[i].m_name;
				if (name != NULL && strings.find(name) != strings.end())
				{
					deleteString((unsigned int)i);
				}
			}
		}
	}
}

Document::Document(Type type, LANGID language)
{
	// Create 'current' version
	m_versions.resize(1);

	m_type        = type;
	m_curLanguage = language;
	m_curVersion  = &m_versions[0];
	m_curValues   = &m_curVersion->m_values[language];
	m_newPostfixes[language];
};
