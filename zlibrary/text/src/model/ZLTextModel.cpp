/*
 * Copyright (C) 2004-2010 Geometer Plus <contact@geometerplus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <cstring>
#include <algorithm>
#include <iostream>

#include <ZLibrary.h>
#include <ZLSearchUtil.h>
#include <ZLLanguageUtil.h>
#include <ZLFile.h>
#include <ZLTextStyleCollection.h>

#include "ZLTextModel.h"
#include "ZLTextParagraph.h"

ZLTextModel::ZLTextModel(const std::string &language, const size_t rowSize) : myLanguage(language.empty() ? ZLibrary::Language() : language), myAllocator(rowSize), myLastEntryStart(0) {
}

ZLTextModel::~ZLTextModel() {
	for (std::vector<ZLTextParagraph*>::const_iterator it = myParagraphs.begin(); it != myParagraphs.end(); ++it) {
		delete *it;
	}
}

const std::string &ZLTextModel::language() const {
	return myLanguage;
}

bool ZLTextModel::isRtl() const {
	return ZLLanguageUtil::isRTLLanguage(myLanguage);
}

void ZLTextModel::search(const std::string &text, size_t startIndex, size_t endIndex, bool ignoreCase) const {
	ZLSearchPattern pattern(text, ignoreCase);
	myMarks.clear();

	std::vector<ZLTextParagraph*>::const_iterator start =
		(startIndex < myParagraphs.size()) ? myParagraphs.begin() + startIndex : myParagraphs.end();
	std::vector<ZLTextParagraph*>::const_iterator end =
		(endIndex < myParagraphs.size()) ? myParagraphs.begin() + endIndex : myParagraphs.end();
	for (std::vector<ZLTextParagraph*>::const_iterator it = start; it < end; ++it) {
		int offset = 0;
		for (ZLTextParagraph::Iterator jt = **it; !jt.isEnd(); jt.next()) {
			if (jt.entryKind() == ZLTextParagraphEntry::TEXT_ENTRY) {
				const ZLTextEntry& textEntry = (ZLTextEntry&)*jt.entry();
				const char *str = textEntry.data();
				const size_t len = textEntry.dataLength();
				for (int pos = ZLSearchUtil::find(str, len, pattern); pos != -1; pos = ZLSearchUtil::find(str, len, pattern, pos + 1)) {
					myMarks.push_back(ZLTextMark(it - myParagraphs.begin(), offset + pos, pattern.length()));
				}
				offset += len;
			}
		}
	}
}

void ZLTextModel::selectParagraph(size_t index) const {
	if (index < paragraphsNumber()) {
		myMarks.push_back(ZLTextMark(index, 0, (*this)[index]->textDataLength()));
	}
}

ZLTextMark ZLTextModel::firstMark() const {
	return marks().empty() ? ZLTextMark() : marks().front();
}

ZLTextMark ZLTextModel::lastMark() const {
	return marks().empty() ? ZLTextMark() : marks().back();
}

ZLTextMark ZLTextModel::nextMark(ZLTextMark position) const {
	std::vector<ZLTextMark>::const_iterator it = std::upper_bound(marks().begin(), marks().end(), position);
	return (it != marks().end()) ? *it : ZLTextMark();
}

ZLTextMark ZLTextModel::previousMark(ZLTextMark position) const {
	if (marks().empty()) {
		return ZLTextMark();
	}
	std::vector<ZLTextMark>::const_iterator it = std::lower_bound(marks().begin(), marks().end(), position);
	if (it == marks().end()) {
		--it;
	}
	if (*it >= position) {
		if (it == marks().begin()) {
			return ZLTextMark();
		}
		--it;
	}
	return *it;
}

ZLTextTreeModel::ZLTextTreeModel(const std::string &language) : ZLTextModel(language, 8192) {
	myRoot = new ZLTextTreeParagraph();
	myRoot->open(true);
}

ZLTextTreeModel::~ZLTextTreeModel() {
	delete myRoot;
}

void ZLTextModel::addParagraphInternal(ZLTextParagraph *paragraph) {
	myParagraphs.push_back(paragraph);
	myLastEntryStart = 0;
}

ZLTextTreeParagraph *ZLTextTreeModel::createParagraph(ZLTextTreeParagraph *parent) {
	if (parent == 0) {
		parent = myRoot;
	}
	ZLTextTreeParagraph *tp = new ZLTextTreeParagraph(parent);
	addParagraphInternal(tp);
	return tp;
}

void ZLTextTreeModel::search(const std::string &text, size_t startIndex, size_t endIndex, bool ignoreCase) const {
	ZLTextModel::search(text, startIndex, endIndex, ignoreCase);
	for (std::vector<ZLTextMark>::const_iterator it = marks().begin(); it != marks().end(); ++it) {
		((ZLTextTreeParagraph*)(*this)[it->ParagraphIndex])->openTree();
	}
}

void ZLTextTreeModel::selectParagraph(size_t index) const {
	if (index < paragraphsNumber()) {
		ZLTextModel::selectParagraph(index);
		((ZLTextTreeParagraph*)(*this)[index])->openTree();
	}
}

ZLTextPlainModel::ZLTextPlainModel(const std::string &language, const size_t rowSize) : ZLTextModel(language, rowSize) {
}

void ZLTextPlainModel::createParagraph(ZLTextParagraph::Kind kind) {
	ZLTextParagraph *paragraph = (kind == ZLTextParagraph::TEXT_PARAGRAPH) ? new ZLTextParagraph() : new ZLTextSpecialParagraph(kind);
	addParagraphInternal(paragraph);
}

void ZLTextModel::addText(const std::string &text) {
	size_t len = text.length();
	if ((myLastEntryStart != 0) && (*myLastEntryStart == ZLTextParagraphEntry::TEXT_ENTRY)) {
		size_t oldLen = 0;
		memcpy(&oldLen, myLastEntryStart + 1, sizeof(size_t));
		size_t newLen = oldLen + len;
		myLastEntryStart = myAllocator.reallocateLast(myLastEntryStart, newLen + sizeof(size_t) + 1);
		memcpy(myLastEntryStart + 1, &newLen, sizeof(size_t));
		memcpy(myLastEntryStart + sizeof(size_t) + 1 + oldLen, text.data(), len);
	} else {
		myLastEntryStart = myAllocator.allocate(len + sizeof(size_t) + 1);
		*myLastEntryStart = ZLTextParagraphEntry::TEXT_ENTRY;
		memcpy(myLastEntryStart + 1, &len, sizeof(size_t));
		memcpy(myLastEntryStart + sizeof(size_t) + 1, text.data(), len);
		myParagraphs.back()->addEntry(myLastEntryStart);
	}
}

void ZLTextModel::addText(const std::vector<std::string> &text) {
	if (text.size() == 0) {
		return;
	}
	size_t len = 0;
	for (std::vector<std::string>::const_iterator it = text.begin(); it != text.end(); ++it) {
		len += it->length();
	}
	if ((myLastEntryStart != 0) && (*myLastEntryStart == ZLTextParagraphEntry::TEXT_ENTRY)) {
		size_t oldLen = 0;
		memcpy(&oldLen, myLastEntryStart + 1, sizeof(size_t));
		size_t newLen = oldLen + len;
		myLastEntryStart = myAllocator.reallocateLast(myLastEntryStart, newLen + sizeof(size_t) + 1);
		memcpy(myLastEntryStart + 1, &newLen, sizeof(size_t));
		size_t offset = sizeof(size_t) + 1 + oldLen;
		for (std::vector<std::string>::const_iterator it = text.begin(); it != text.end(); ++it) {
			memcpy(myLastEntryStart + offset, it->data(), it->length());
			offset += it->length();
		}
	} else {
		myLastEntryStart = myAllocator.allocate(len + sizeof(size_t) + 1);
		*myLastEntryStart = ZLTextParagraphEntry::TEXT_ENTRY;
		memcpy(myLastEntryStart + 1, &len, sizeof(size_t));
		size_t offset = sizeof(size_t) + 1;
		for (std::vector<std::string>::const_iterator it = text.begin(); it != text.end(); ++it) {
			memcpy(myLastEntryStart + offset, it->data(), it->length());
			offset += it->length();
		}
		myParagraphs.back()->addEntry(myLastEntryStart);
	}
}

void ZLTextModel::addFixedHSpace(unsigned char length) {
	myLastEntryStart = myAllocator.allocate(2);
	*myLastEntryStart = ZLTextParagraphEntry::FIXED_HSPACE_ENTRY;
	*(myLastEntryStart + 1) = length;
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::addControl(ZLTextKind textKind, bool isStart) {
	myLastEntryStart = myAllocator.allocate(2);
	*myLastEntryStart = ZLTextParagraphEntry::CONTROL_ENTRY;
	*(myLastEntryStart + 1) = (textKind << 1) + (isStart ? 1 : 0);
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::addControl(const ZLTextStyleEntry &entry) {
	int len = sizeof(int) + 5 + ZLTextStyleEntry::NUMBER_OF_LENGTHS * (sizeof(short) + 1);
	if (entry.fontFamilySupported()) {
		len += entry.fontFamily().length() + 1;
	}
	myLastEntryStart = myAllocator.allocate(len);
	char *address = myLastEntryStart;
	*address++ = ZLTextParagraphEntry::STYLE_ENTRY;
	memcpy(address, &entry.myMask, sizeof(int));
	address += sizeof(int);
	for (int i = 0; i < ZLTextStyleEntry::NUMBER_OF_LENGTHS; ++i) {
		*address++ = entry.myLengths[i].Unit;
		memcpy(address, &entry.myLengths[i].Size, sizeof(short));
		address += sizeof(short);
	}
	*address++ = entry.mySupportedFontModifier;
	*address++ = entry.myFontModifier;
	*address++ = entry.myAlignmentType;
	*address++ = entry.myFontSizeMag;
	if (entry.fontFamilySupported()) {
		memcpy(address, entry.fontFamily().data(), entry.fontFamily().length());
		address += entry.fontFamily().length();
		*address++ = '\0';
	}
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::addHyperlinkControl(ZLTextKind textKind, const std::string &label, const std::string &hyperlinkType) {
	myLastEntryStart = myAllocator.allocate(label.length() + hyperlinkType.length() + 4);
	*myLastEntryStart = ZLTextParagraphEntry::HYPERLINK_CONTROL_ENTRY;
	*(myLastEntryStart + 1) = textKind;
	memcpy(myLastEntryStart + 2, label.data(), label.length());
	*(myLastEntryStart + label.length() + 2) = '\0';
	memcpy(myLastEntryStart + label.length() + 3, hyperlinkType.data(), hyperlinkType.length());
	*(myLastEntryStart + label.length() + hyperlinkType.length() + 3) = '\0';
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::addImage(const std::string &id, const ZLImageMap &imageMap, short vOffset) {
	myLastEntryStart = myAllocator.allocate(sizeof(const ZLImageMap*) + sizeof(short) + id.length() + 2);
	*myLastEntryStart = ZLTextParagraphEntry::IMAGE_ENTRY;
	const ZLImageMap *imageMapAddress = &imageMap;
	memcpy(myLastEntryStart + 1, &imageMapAddress, sizeof(const ZLImageMap*));
	memcpy(myLastEntryStart + 1 + sizeof(const ZLImageMap*), &vOffset, sizeof(short));
	memcpy(myLastEntryStart + 1 + sizeof(const ZLImageMap*) + sizeof(short), id.data(), id.length());
	*(myLastEntryStart + 1 + sizeof(const ZLImageMap*) + sizeof(short) + id.length()) = '\0';
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::addBidiReset() {
	myLastEntryStart = myAllocator.allocate(1);
	*myLastEntryStart = ZLTextParagraphEntry::RESET_BIDI_ENTRY;
	myParagraphs.back()->addEntry(myLastEntryStart);
}

void ZLTextModel::dumpToString(const ZLFile &file, std::ostream &out) {
    out << "dumpTextModel:" << file.path()
        << " " << myParagraphs.size() << std::endl;

    for (auto p : myParagraphs) {
        std::string kind;
        switch (p->kind()) {
            case ZLTextParagraph::TEXT_PARAGRAPH:
                kind = "text";
                break;
            case ZLTextParagraph::TREE_PARAGRAPH:
                kind = "tree";
                break;
            default:
                kind = "UNKNOWN";
        }

        out << std::endl
            << "paragraph{ "
            << "kind:" << kind
            << ", entryNum:" << p->entryNumber()
            << ", charNum:" << p->characterNumber()
            << ", dataLen:" << p->textDataLength()
            << " }" << std::endl;

        for (ZLTextParagraph::Iterator pit = *p; !pit.isEnd(); pit.next()) {
            const shared_ptr<ZLTextParagraphEntry> &entry = pit.entry();

#define __CAST(TYPE) dynamic_cast<TYPE *>(&(*entry))
            if (auto *text = __CAST(ZLTextEntry)) {
                out << "text[" << text->data() << "]" << std::endl;
            } else if (auto *control = __CAST(ZLTextControlEntry)) {
                auto deco = ZLTextStyleCollection::Instance().decoration(control->kind());
                std::string name;
                if (deco != nullptr) {
                    name = deco->name();
                }
                out << "control["
                    << static_cast<int>(control->kind()) << ", " << name << ", " << control->isHyperlink() << ", " << control->isStart()
                    << "]" << std::endl;
            } else if (auto *link = __CAST(ZLTextHyperlinkControlEntry)) {
                out << "link["
                    << link->kind() << ", " << link->label() << ", " << link->hyperlinkType()
                    << "]" << std::endl;
            } else if (auto *image = __CAST(ImageEntry)) {
                out << "image["
                    << image->id()
                    << "]" << std::endl;
            } else if (auto *style = __CAST(ZLTextStyleEntry)) {
                std::string align;
                switch (style->alignmentType()) {
                    case ZLTextAlignmentType::ALIGN_CENTER:
                        align = "center";
                        break;
                    case ZLTextAlignmentType::ALIGN_JUSTIFY:
                        align = "justify";
                        break;
                    case ZLTextAlignmentType::ALIGN_LEFT:
                        align = "left";
                        break;
                    case ZLTextAlignmentType::ALIGN_RIGHT:
                        align = "right";
                        break;
                    case ZLTextAlignmentType::ALIGN_LINESTART:
                        align = "linestart";
                        break;
                    case ZLTextAlignmentType::ALIGN_UNDEFINED:
                        align = "undefined";
                        break;
                }
                out << "style["
                    << "font-family:" << style->fontFamily() << ", "
                    << "align:" << align << ", "
                    << "font-modifier:" << static_cast<int>(style->fontModifier()) << ", "
                    << "font-size-mag:" << static_cast<int>(style->fontSizeMag())
                    << "]" << std::endl;
            } else if (auto *fixh = __CAST(ZLTextFixedHSpaceEntry)) {
                out << "fixh["
                    << fixh->length()
                    << "]" << std::endl;
            } else if (auto *resetBidi = __CAST(ResetBidiEntry)) {
                out << "resetBidi" << std::endl;
            }
#undef __CAST

        }
    }
}
