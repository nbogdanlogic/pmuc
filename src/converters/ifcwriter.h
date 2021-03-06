/*
 * Plant Mock-Up Converter
 *
 * Copyright (c) 2019, EDF. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

#ifndef IFCWRITER_H
#define IFCWRITER_H

#include <boost/locale/encoding_utf.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <variant>

#define UNDEFINED_TEXT '*'
#define IFC_STRING_UNSET "$"

using boost::locale::conv::utf_to_utf;

inline std::wstring utf8_to_wstring(const std::string& str) {
  return utf_to_utf<wchar_t>(str.c_str(), str.c_str() + str.size());
}

// (Legally) stolen and adapted from IFCPlusPlus
inline std::string encodeSTEPString(std::string utf8) {
  std::wstring str = utf8_to_wstring(utf8);
  wchar_t* stream_pos = (wchar_t*)str.c_str();
  std::string result_str;
  std::string beginUnicodeTag = "\\X2\\";
  std::string endUnicodeTag = "\\X0\\";
  bool hasOpenedUnicodeTag = false;

  while (*stream_pos != '\0') {
    wchar_t append_char = *stream_pos;
    if (append_char > 0 && append_char < 128) {
      if (hasOpenedUnicodeTag) {
        result_str += endUnicodeTag;
        hasOpenedUnicodeTag = false;
      }
      if (append_char == '\\') {
        result_str += "\\\\";
      } else {
        result_str.push_back((char)append_char);
      }
    } else {
      int value = (int)(append_char);
      wchar_t temporary[8];
      swprintf(temporary, 5, L"%04X", value);

      if (!hasOpenedUnicodeTag) {
        result_str += beginUnicodeTag;
        hasOpenedUnicodeTag = true;
      }

      char mb[8];
      wctomb(mb, temporary[0]);
      result_str.push_back(mb[0]);
      wctomb(mb, temporary[1]);
      result_str.push_back(mb[0]);
      wctomb(mb, temporary[2]);
      result_str.push_back(mb[0]);
      wctomb(mb, temporary[3]);
      result_str.push_back(mb[0]);
    }
    ++stream_pos;
  }

  if (hasOpenedUnicodeTag) {
    result_str += endUnicodeTag;
    hasOpenedUnicodeTag = false;
  }

  return result_str;
}

struct IfcEnum {
  std::string value;

  IfcEnum() : value(0){};
  IfcEnum(std::string v) : value(v){};
};

const IfcEnum IFC_FALSE = IfcEnum{"F"};
const IfcEnum IFC_TRUE = IfcEnum{"T"};
const IfcEnum IFCREFLECTANCEMETHOD_BLINN = IfcEnum{"BLINN"};
const IfcEnum IFCSURFACESIDE_BOTH = IfcEnum{"BOTH"};
const IfcEnum IFCUNIT_LENGTHUNIT = IfcEnum{"LENGTHUNIT"};
const IfcEnum IFCUNIT_PLANEANGLEUNIT = IfcEnum{"PLANEANGLEUNIT"};
const IfcEnum IFCSIUNITNAME_METRE = IfcEnum{"METRE"};
const IfcEnum IFCSIUNITNAME_RADIAN = IfcEnum{"RADIAN"};
const IfcEnum IFCCHANGEACTIONENUM_NOCHANGE = IfcEnum{"NOCHANGE"};
const IfcEnum IFCELEMENTCOMPOSITION_ELEMENT = IfcEnum{"ELEMENT"};
const IfcEnum IFCPROFILETYPE_AREA = IfcEnum{"AREA"};
const IfcEnum IFCTRANSITIONCODE_CONTINUOUS = IfcEnum{"CONTINUOUS"};
const IfcEnum IFCTRIMMINGPREFERENCE_CARTESIAN = IfcEnum{"CARTESIAN"};
const IfcEnum IFCBOOLEANOPERATOR_DIFFERENCE = IfcEnum{"DIFFERENCE"};

typedef std::string IfcString;
typedef std::vector<IfcString> IfcStringList;
typedef int IfcInteger;
typedef float IfcFloat;
typedef std::vector<IfcFloat> IfcFloatList;

struct IfcSimpleValue {
  std::string value;
  std::string type;

  IfcSimpleValue() : value(0){};
  IfcSimpleValue(std::string v) : value(v), type("IFCLABEL"){};
  IfcSimpleValue(std::string v, std::string t) : value(v), type(t){};
};

struct IfcReference {
  unsigned int value;

  IfcReference() : value(0){};
  IfcReference(unsigned int v) : value(v){};
};
const IfcReference IFC_REFERENCE_UNSET = IfcReference{};
typedef std::vector<IfcReference> IfcReferenceList;

typedef std::variant<IfcString,
                     IfcStringList,
                     IfcReference,
                     IfcReferenceList,
                     IfcInteger,
                     IfcFloat,
                     IfcFloatList,
                     IfcSimpleValue,
                     IfcEnum>
    IfcValue;
typedef std::vector<IfcValue> IfcValueList;

struct FileDescription {
  std::vector<std::string> description;
  std::string implementationLevel;

  FileDescription() : implementationLevel("2;1") {}
};

struct FileName {
  std::string name;
  std::string time_stamp_text;
  std::vector<std::string> author;
  std::vector<std::string> organization;
  std::string preprocessor_version;
  std::string originating_system;
  std::string authorization;

  FileName() {}
};

struct FileSchema {
  std::vector<std::string> schema_identifiers = {"IFC2X3"};
};

struct IfcEntity {
  std::string name;
  IfcValueList attributes;

  IfcEntity(const std::string& name) : name(name){};
};

class IFCStreamWriter {
  struct AttributeVisitor {
    IFCStreamWriter* writer;
    bool lastAttribute;

    void operator()(const IfcString& s) const { writer->addString(s, lastAttribute); }
    void operator()(const IfcStringList& l) const { writer->addStringList(l, lastAttribute); }
    void operator()(const IfcReference& r) const { writer->addReference(r.value, lastAttribute); }
    void operator()(const IfcReferenceList& l) const { writer->addReferenceList(l, lastAttribute); }
    void operator()(IfcInteger i) const { writer->addNumber(i, lastAttribute); }
    void operator()(IfcFloat f) const { writer->addNumber(f, lastAttribute); }
    void operator()(IfcFloatList fl) const { writer->addNumberList(fl, lastAttribute); }
    void operator()(IfcSimpleValue sv) const { writer->addSimpleValue(sv, lastAttribute); }
    void operator()(IfcEnum e) const { writer->addEnumeration(e, lastAttribute); }

    AttributeVisitor(IFCStreamWriter* writer, bool lastAttribute) : writer(writer), lastAttribute(lastAttribute){};
  };

 public:
  IFCStreamWriter(const std::string& fileName) : mFileName(fileName), mEntityNumber(1) {
    mFile.open(fileName, std::ios::out);
  };

  void startDocument() { mFile << "ISO-10303-21;" << std::endl; }

  void endDocument() {
    mFile << "ENDSEC;" << std::endl;
    mFile << "END-ISO-10303-21;" << std::endl;
  }

  void addHeader(const FileDescription& desc, const FileName& name, const FileSchema& schema = FileSchema()) {
    mFile << "HEADER;" << std::endl;
    startEntity("FILE_DESCRIPTION", false);
    addStringList(desc.description);
    addString(desc.implementationLevel, true);
    closeEntity();

    startEntity("FILE_NAME", false);
    addString(name.name);
    addString(name.time_stamp_text);
    addStringList(name.author);
    addStringList(name.organization);
    addString(name.preprocessor_version);
    addString(name.originating_system);
    addString(name.authorization, true);
    closeEntity();

    startEntity("FILE_SCHEMA", false);
    addStringList(schema.schema_identifiers, true);
    closeEntity();

    mFile << "ENDSEC;" << std::endl;
    mFile << "DATA;" << std::endl;
  }

  IfcReference addEntity(const IfcEntity& entity) {
    auto number = startEntity(entity.name);
    auto attributes = entity.attributes;

    for (IfcValueList::const_iterator p = attributes.begin(); p != attributes.end(); ++p) {
      std::visit(AttributeVisitor(this, p == attributes.end() - 1), *p);
    }
    closeEntity();
    return IfcReference(number);
  }

  void addFileHeader(const std::string& header) { mFile << header; }

  void addAttributeSeparator() { mFile << ","; }

  void addString(const std::string& str, bool lastAttribute = false) {
    if (str == IFC_STRING_UNSET) {
      mFile << str;
    } else {
      mFile << "'" << str << "'";
    }
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  void addEnumeration(const IfcEnum& e, bool lastAttribute = false) {
    mFile << "." << e.value << ".";
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  void addStringList(const std::vector<std::string>& list, bool lastAttribute = false) {
    mFile << "(";
    if (list.empty()) {
      mFile << "''";
    }
    for (std::vector<std::string>::const_iterator p = list.begin(); p != list.end(); ++p) {
      addString(*p, p == list.end() - 1);
    }
    mFile << ")";
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  void addReference(unsigned long id, bool lastAttribute = false) {
    if (!id) {
      mFile << "*";
      if (!lastAttribute) {
        addAttributeSeparator();
      }

      return;
    }
    mFile << "#";
    addNumber(id, lastAttribute);
  }

  void addReferenceList(const IfcReferenceList& list, bool lastAttribute = false) {
    mFile << "(";
    if (list.empty()) {
      mFile << "..";
    }
    for (auto p = list.begin(); p != list.end(); ++p) {
      addReference(p->value, p == list.end() - 1);
    }
    mFile << ")";
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  void addSimpleValue(const IfcSimpleValue& sv, bool lastAttribute = false) {
    if (sv.type == "IFCLABEL") {
      mFile << sv.type << "('" << encodeSTEPString(sv.value) << "')";
    } else {
      mFile << sv.type << "(" << sv.value << ")";
    }
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  template <typename T>
  void addNumber(const T n, bool lastAttribute = false) {
    mFile << std::to_string(n);
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  template <typename T>
  void addNumberList(const std::vector<T>& list, bool lastAttribute = false) {
    mFile << "(";
    for (auto p = list.begin(); p != list.end(); ++p) {
      addNumber(*p, p == list.end() - 1);
    }
    mFile << ")";
    if (!lastAttribute) {
      addAttributeSeparator();
    }
  }

  unsigned long startEntity(const std::string& name, bool numbered = true) {
    unsigned long entityNumber = 0;
    if (numbered) {
      entityNumber = mEntityNumber++;
      mFile << "#" << std::to_string(entityNumber) << "= ";
    }
    mFile << name << "(";
    return entityNumber;
  }

  void closeEntity() { mFile << ");" << std::endl; }

  std::string mFileName;
  std::ofstream mFile;
  unsigned long mEntityNumber;
};

// (Legally) stolen and adapted from IFCPlusPlus
///@brief Creates a GUID string with 36 characters including dashes, for example: "F103000C-9865-44EE-BE6E-CCC780B81423"
template <typename T>
inline std::basic_string<T> createGUID32() {
  std::basic_stringstream<T> uuid_strs;
  uuid_strs << std::uppercase;
  boost::uuids::uuid uuid = boost::uuids::random_generator()();
  uuid_strs << uuid;
  return uuid_strs.str();
}

static const char base16mask[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,
    4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const char base64mask[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 63, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,
    4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, 62, -1, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1};

///@brief Compresses a GUID string
///@details Expects a string with exactly 36 characters including dashes, for example:
///"F103000C-9865-44EE-BE6E-CCC780B81423"
///@returns an IFC GUID string with 22 characters, for example: "3n0m0Cc6L4xhvkpCU0k1GZ"
template <typename T>
inline std::basic_string<T> compressGUID(const std::basic_string<T>& in) {
  static constexpr std::array<T, 64> base64Chars = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
      'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
      'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '_', '$'};

  std::basic_string<T> temp;
  std::basic_string<T> result;
  result.resize(23);
  result[0] = '0';

  temp.push_back('0');

  // remove dashes
  for (size_t ii = 0; ii < in.length(); ++ii) {
    if (in[ii] != '-') {
      temp.push_back(in[ii]);
    }
  }

  // compress
  int n = 0;
  for (size_t ii_out = 0, ii = 0; ii < 32; ii += 3) {
    n = base16mask[temp[ii]] << 8;
    n += base16mask[temp[ii + 1]] << 4;
    n += base16mask[temp[ii + 2]];
    result[ii_out + 1] = base64Chars[n % 64];
    result[ii_out] = base64Chars[n / 64];
    ii_out += 2;
  }
  result.resize(22);
  return result;
}

template <typename T>
inline std::basic_string<T> createBase64Uuid() {
  std::basic_string<T> guid_uncompressed = createGUID32<T>();
  std::basic_string<T> guid_compressed = compressGUID<T>(guid_uncompressed);
  return guid_compressed;
}

#endif