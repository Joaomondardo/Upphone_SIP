/*
 * Upphone SIP - XML of the contacts (Contacts.xml, export) and of the users directory
 * Copyright (C) 2026 Upp Tecnologia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <utility>
#include <vector>

// Small XML reader and writer of the app, in place of the CMarkup library (whose license does not allow
// distributing its source). It reads the files the app always read, in the same way:
//   - only the entities &amp; &lt; &gt; &quot; &apos; are decoded; anything else (&#233;, &nbsp;, a lone &)
//     stays as written, and line breaks inside attribute values are kept;
//   - names are case sensitive; attributes without quotes or without a value are ignored; a repeated
//     attribute keeps its first value;
//   - comments, processing instructions, DOCTYPE (no entity is expanded) and CDATA between elements are skipped;
//   - only the first element of the document is read (what comes after it is ignored);
//   - an end tag may have spaces after "</" and anything between its name and '>';
//   - a document cut before the end of the first element, or with an end tag that does not match, is refused.
// Differences from CMarkup, all with broken documents: a '>' inside a quoted attribute value is accepted (valid
// XML that CMarkup refused); an attribute value whose quote is never closed, or a tag without a name ("<>"),
// makes the document invalid (CMarkup read a partial or empty list, which would replace the users directory);
// and "< ?" or "< !" do not make the app hang (CMarkup never returned).
namespace Xml {

struct Element {
    CString name;
    std::vector<std::pair<CString, CString>> attributes; // in document order
    std::vector<Element> children;                       // child elements, in document order
    // content of an element without child elements (entities decoded; a content made of one CDATA section
    // gives the text inside it); "" when it has child elements
    CString text;

    const CString* Attribute(LPCTSTR attr) const; // nullptr when the element does not have it
    CString AttributeValue(LPCTSTR attr) const;   // "" when missing
    // index of the first child named childName after the child `after` (-1: from the first one), or -1
    int NextChild(int after, LPCTSTR childName) const;
};

// Reads the first element of the document. false: no element, document cut or end tag that does not match;
// error (optional) then says why and where ("attribute value not closed (character 120)").
bool Parse(const CString& doc, Element& root, CString* error = nullptr);

// Document made of a root element with child elements that only have attributes, written as before:
// "<root>\r\n<child a=\"1\" b=\"2\"/>\r\n</root>\r\n", or "<root/>\r\n" without children (values with
// & < > " ' escaped; line breaks are kept as they are).
class Writer {
public:
    explicit Writer(LPCTSTR root);
    void AddChild(LPCTSTR name);
    void AddAttribute(LPCTSTR name, const CString& value); // to the last child added
    CString GetDoc() const;

private:
    CString m_root;
    CString m_children; // children already written
    bool m_childOpen = false;
};

} // namespace Xml
