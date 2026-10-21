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

#include "stdafx.h"
#include "Xml.h"

namespace Xml {

namespace {

// a users directory comes from a server: a document nested deeper than this is refused
const size_t kMaxDepth = 256;

bool IsSpace(TCHAR c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// end of an element name, as CMarkup read it ("<contact<x", "<contact=x" and "</contact x>" name "contact")
bool EndsName(TCHAR c)
{
    return IsSpace(c) || c == '>' || c == '/' || c == '<' || c == '=';
}

// &amp; &lt; &gt; &quot; &apos;; anything else stays as written
CString Decode(LPCTSTR p, int length)
{
    static const struct { LPCTSTR text; int length; TCHAR ch; } entities[] = {
        { _T("&amp;"), 5, '&' }, { _T("&lt;"), 4, '<' }, { _T("&gt;"), 4, '>' }, { _T("&quot;"), 6, '"' }, { _T("&apos;"), 6, '\'' },
    };
    CString out;
    out.Preallocate(length);
    for (int i = 0; i < length; i++) {
        TCHAR c = p[i];
        if (c == '&') {
            for (const auto& entity : entities) {
                if (i + entity.length <= length && _tcsncmp(p + i, entity.text, entity.length) == 0) {
                    c = entity.ch;
                    i += entity.length - 1;
                    break;
                }
            }
        }
        out.AppendChar(c);
    }
    return out;
}

// content of an element without child elements: one CDATA section gives the text inside it
CString Content(LPCTSTR p, int length)
{
    const int open = 9;  // <![CDATA[
    const int close = 3; // ]]>
    if (length >= open + close && _tcsncmp(p, _T("<![CDATA["), open) == 0
        && _tcsncmp(p + length - close, _T("]]>"), close) == 0) {
        LPCTSTR inner = p + open;
        int innerLength = length - open - close;
        bool single = true;
        for (int i = 0; i + close <= innerLength; i++) {
            if (_tcsncmp(inner + i, _T("]]>"), close) == 0) {
                single = false;
                break;
            }
        }
        if (single) {
            return CString(inner, innerLength);
        }
    }
    return Decode(p, length);
}

CString Escape(const CString& value)
{
    CString out;
    out.Preallocate(value.GetLength());
    for (int i = 0; i < value.GetLength(); i++) {
        TCHAR c = value[i];
        switch (c) {
        case '&': out += _T("&amp;"); break;
        case '<': out += _T("&lt;"); break;
        case '>': out += _T("&gt;"); break;
        case '"': out += _T("&quot;"); break;
        case '\'': out += _T("&apos;"); break;
        default: out.AppendChar(c);
        }
    }
    return out;
}

} // namespace

const CString* Element::Attribute(LPCTSTR attr) const
{
    for (const auto& attribute : attributes) {
        if (attribute.first == attr) {
            return &attribute.second;
        }
    }
    return nullptr;
}

CString Element::AttributeValue(LPCTSTR attr) const
{
    const CString* value = Attribute(attr);
    return value ? *value : CString();
}

int Element::NextChild(int after, LPCTSTR childName) const
{
    for (int i = after < 0 ? 0 : after + 1; i < (int)children.size(); i++) {
        if (children[i].name == childName) {
            return i;
        }
    }
    return -1;
}

bool Parse(const CString& doc, Element& root, CString* error)
{
    root = Element();
    LPCTSTR p = doc;
    const int n = doc.GetLength();
    int i = 0;
    // open elements (the last one receives the children) and where the content of each one starts. The
    // pointers stay valid: only the last open element gets new children while it is open.
    std::vector<Element*> open;
    std::vector<int> contentStart;
    bool haveRoot = false;

    auto fail = [&](LPCTSTR reason, int at) {
        if (error) {
            error->Format(_T("%s (character %d)"), reason, at + 1);
        }
        return false;
    };
    auto startsWith = [&](LPCTSTR text) {
        int length = (int)_tcslen(text);
        return i + length <= n && _tcsncmp(p + i, text, length) == 0;
    };
    // moves past the next `end`; false when the document ends before it
    auto skipPast = [&](LPCTSTR end) {
        int length = (int)_tcslen(end);
        for (int j = i; j + length <= n; j++) {
            if (_tcsncmp(p + j, end, length) == 0) {
                i = j + length;
                return true;
            }
        }
        return false;
    };

    while (i < n) {
        if (p[i] != '<') {
            i++; // text: part of the content of the open element
            continue;
        }
        int tagStart = i;
        if (startsWith(_T("<!--"))) {
            i += 2; // "-->" may share the dashes of "<!--" ("<!--->" is a whole comment, as for CMarkup)
            if (!skipPast(_T("-->"))) {
                return fail(_T("comment not closed"), tagStart);
            }
            continue;
        }
        if (startsWith(_T("<![CDATA["))) {
            i += 9;
            if (!skipPast(_T("]]>"))) {
                return fail(_T("CDATA section not closed"), tagStart);
            }
            continue;
        }
        if (startsWith(_T("<?"))) {
            i += 1; // "?>" may share the '?' of "<?" ("<?>" is a whole instruction, as for CMarkup)
            if (!skipPast(_T("?>"))) {
                return fail(_T("processing instruction not closed"), tagStart);
            }
            continue;
        }
        if (startsWith(_T("<!"))) {
            // DOCTYPE or another declaration: up to the first '>', or, when a '[' (internal subset) comes
            // before it, up to the first '>' after the first ']'
            i += 2;
            while (i < n && p[i] != '>' && p[i] != '[') {
                i++;
            }
            if (i < n && p[i] == '[') {
                while (i < n && p[i] != ']') {
                    i++;
                }
                while (i < n && p[i] != '>') {
                    i++;
                }
            }
            if (i >= n) {
                return fail(_T("declaration not closed"), tagStart);
            }
            i++;
            continue;
        }
        int afterLt = i + 1; // "< /contacts>" is an end tag too
        while (afterLt < n && IsSpace(p[afterLt])) {
            afterLt++;
        }
        if (afterLt < n && p[afterLt] == '/') {
            // end tag, read as CMarkup did: spaces around "/" and anything between the name and '>' are accepted
            i = afterLt + 1;
            while (i < n && IsSpace(p[i])) {
                i++;
            }
            int nameStart = i;
            while (i < n && !EndsName(p[i])) {
                i++;
            }
            CString name(p + nameStart, i - nameStart);
            while (i < n && p[i] != '>') {
                i++;
            }
            if (i >= n) {
                return fail(_T("end tag cut at the end of the document"), tagStart);
            }
            if (open.empty() || open.back()->name != name) {
                return fail(_T("end tag that does not match the open element"), tagStart);
            }
            i++;
            Element* element = open.back();
            if (element->children.empty()) {
                element->text = Content(p + contentStart.back(), tagStart - contentStart.back());
            }
            open.pop_back();
            contentStart.pop_back();
            if (open.empty()) {
                return true; // end of the first element: the rest of the document is not read
            }
            continue;
        }

        // start tag
        i++;
        while (i < n && IsSpace(p[i])) {
            i++;
        }
        int nameStart = i;
        while (i < n && !EndsName(p[i])) {
            i++;
        }
        Element element;
        element.name = CString(p + nameStart, i - nameStart);
        bool closed = false;
        bool selfClosing = false;
        while (i < n) {
            TCHAR c = p[i];
            if (IsSpace(c)) {
                i++;
                continue;
            }
            if (c == '>') {
                i++;
                closed = true;
                break;
            }
            if (c == '/') {
                i++;
                if (i < n && p[i] == '>') {
                    i++;
                    closed = selfClosing = true;
                    break;
                }
                continue; // a '/' that does not end the tag is skipped
            }
            int attrStart = i;
            while (i < n && !IsSpace(p[i]) && p[i] != '=' && p[i] != '>' && p[i] != '/') {
                i++;
            }
            CString attrName(p + attrStart, i - attrStart);
            int j = i;
            while (j < n && IsSpace(p[j])) {
                j++;
            }
            if (j >= n || p[j] != '=') {
                continue; // attribute without a value: ignored
            }
            i = j + 1;
            while (i < n && IsSpace(p[i])) {
                i++;
            }
            if (i < n && (p[i] == '"' || p[i] == '\'')) {
                TCHAR quote = p[i++];
                int valueStart = i;
                while (i < n && p[i] != quote) {
                    i++;
                }
                if (i >= n) {
                    return fail(_T("attribute value not closed"), valueStart - 1);
                }
                if (!attrName.IsEmpty()) {
                    element.attributes.emplace_back(attrName, Decode(p + valueStart, i - valueStart));
                }
                i++;
            }
            else {
                // value without quotes: ignored
                while (i < n && !IsSpace(p[i]) && p[i] != '>' && !(p[i] == '/' && i + 1 < n && p[i + 1] == '>')) {
                    i++;
                }
            }
        }
        if (!closed) {
            return fail(_T("tag cut at the end of the document"), tagStart);
        }
        if (!haveRoot) {
            haveRoot = true;
            root = std::move(element);
            if (selfClosing) {
                return true;
            }
            open.push_back(&root);
            contentStart.push_back(i);
            continue;
        }
        if (open.size() >= kMaxDepth) {
            return fail(_T("elements nested too deep"), tagStart);
        }
        Element* parent = open.back();
        parent->children.push_back(std::move(element));
        if (!selfClosing) {
            open.push_back(&parent->children.back());
            contentStart.push_back(i);
        }
    }
    return haveRoot ? fail(_T("document cut before the end of the first element"), n - 1) : fail(_T("no element"), 0);
}

Writer::Writer(LPCTSTR root)
    : m_root(root)
{
}

void Writer::AddChild(LPCTSTR name)
{
    if (m_childOpen) {
        m_children += _T("/>\r\n");
    }
    m_children.AppendFormat(_T("<%s"), name);
    m_childOpen = true;
}

void Writer::AddAttribute(LPCTSTR name, const CString& value)
{
    if (m_childOpen) {
        m_children.AppendFormat(_T(" %s=\"%s\""), name, Escape(value).GetString());
    }
}

CString Writer::GetDoc() const
{
    CString doc;
    if (!m_childOpen) {
        doc.Format(_T("<%s/>\r\n"), m_root.GetString()); // no child
        return doc;
    }
    doc.Format(_T("<%s>\r\n%s/>\r\n</%s>\r\n"), m_root.GetString(), m_children.GetString(), m_root.GetString());
    return doc;
}

} // namespace Xml
