/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "storytextsnapshot.h"
#include "storytext.h"
#include "styles/charstyle.h"

StoryTextSnapshot StoryTextSnapshot::create(const StoryText& story)
{
	StoryTextSnapshot snapshot;

	int storyLength = story.length();
	if (storyLength == 0)
		return snapshot;

	// Build everything in a single pass over the StoryText.
	// We skip ignorable code points (soft hyphens, etc.) so that
	// plainText, paragraph boundaries, and language runs all use
	// consistent positions in the filtered text.

	snapshot.plainText.reserve(storyLength);
	snapshot.toOriginal.reserve(storyLength);
	snapshot.paragraphs.reserve(story.nrOfParagraphs());
	snapshot.languages.reserve(storyLength / 10);

	int outPos = 0;        // current position in filtered plainText
	int paraStart = 0;     // start of current paragraph in filtered text
	int langRunStart = 0;  // start of current language run in filtered text
	QString currentLang;

	for (int i = 0; i < storyLength; ++i)
	{
		QChar ch = story.text(i);

		// Handle paragraph separators
		if (ch == SpecialChars::PARSEP)
		{
			// Close the current paragraph
			snapshot.paragraphs.append(ParagraphInfo(paraStart, outPos - paraStart));

			// Close the current language run at the paragraph boundary
			if (!currentLang.isEmpty() && outPos > langRunStart)
			{
				snapshot.languages.append(
					LanguageRun(langRunStart, outPos - langRunStart, currentLang)
				);
			}

			// Emit a newline in the output
			snapshot.plainText += QChar('\n');
			snapshot.toOriginal.append(i);
			++outPos;

			// Start new paragraph and language run
			paraStart = outPos;
			langRunStart = outPos;
			currentLang.clear(); // will be set by next character
			continue;
		}

		// Skip ignorable code points (soft hyphens, etc.)
		if (SpecialChars::isIgnorableCodePoint(ch.unicode()))
			continue;

		// Track language changes
		const CharStyle& style = story.charStyle(i);
		QString lang = style.language();

		if (currentLang.isNull())
		{
			// First character of a paragraph or of the text
			currentLang = lang;
			langRunStart = outPos;
		}
		else if (lang != currentLang)
		{
			// Language changed — close the previous run
			if (!currentLang.isEmpty() && outPos > langRunStart)
			{
				snapshot.languages.append(
					LanguageRun(langRunStart, outPos - langRunStart, currentLang)
				);
			}
			currentLang = lang;
			langRunStart = outPos;
		}

		snapshot.plainText += ch;
		snapshot.toOriginal.append(i);
		++outPos;
	}

	// Close the final paragraph (text may not end with PARSEP)
	if (outPos > paraStart)
		snapshot.paragraphs.append(ParagraphInfo(paraStart, outPos - paraStart));

	// Close the final language run
	if (!currentLang.isEmpty() && outPos > langRunStart)
		snapshot.languages.append(LanguageRun(langRunStart, outPos - langRunStart, currentLang));

	return snapshot;
}

QString StoryTextSnapshot::getParagraphText(int paraIndex) const
{
	if (paraIndex < 0 || paraIndex >= paragraphs.size())
		return QString();
	
	const ParagraphInfo& para = paragraphs[paraIndex];
	
	// Make sure we don't go beyond the text length
	int length = qMin(para.length, plainText.length() - para.start);
	if (length <= 0)
		return QString();
	
	return plainText.mid(para.start, length);
}

QString StoryTextSnapshot::getLanguageAt(int pos) const
{
	if (pos < 0 || pos >= plainText.length())
		return QString();
	
	// Find the language run that contains this position
	for (const LanguageRun& run : languages)
	{
		if (run.contains(pos))
			return run.language;
	}
	
	return QString(); // No language found at this position
}

QVector<LanguageRun> StoryTextSnapshot::getLanguageRunsForParagraph(int paraIndex) const
{
	if (paraIndex < 0 || paraIndex >= paragraphs.size())
		return QVector<LanguageRun>();
	
	const ParagraphInfo& para = paragraphs[paraIndex];
	return getLanguageRunsInRange(para.start, para.end());
}

QVector<LanguageRun> StoryTextSnapshot::getLanguageRunsInRange(int start, int end) const
{
	QVector<LanguageRun> result;
	
	for (const LanguageRun& run : languages)
	{
		// Check if this run intersects with the requested range
		int runStart = run.start;
		int runEnd = run.end();
		
		if (runEnd <= start || runStart >= end)
			continue; // No intersection
		
		// Calculate the intersection
		int intersectStart = qMax(runStart, start);
		int intersectEnd = qMin(runEnd, end);
		
		// Create a new run for the intersection
		// Note: positions are adjusted relative to the original range
		LanguageRun intersectRun;
		intersectRun.start = intersectStart;
		intersectRun.length = intersectEnd - intersectStart;
		intersectRun.language = run.language;
		
		result.append(intersectRun);
	}
	
	return result;
}

int StoryTextSnapshot::getParagraphIndexAt(int pos) const
{
	if (pos < 0 || pos >= plainText.length())
		return -1;
	
	// Binary search for efficiency with large documents
	int left = 0;
	int right = paragraphs.size() - 1;
	
	while (left <= right)
	{
		int mid = left + (right - left) / 2;
		const ParagraphInfo& para = paragraphs[mid];
		
		if (pos < para.start)
			right = mid - 1;
		else if (pos >= para.end())
			left = mid + 1;
		else
			return mid; // Found it
	}
	
	return -1; // Not found (shouldn't happen with valid input)
}

int StoryTextSnapshot::mapToOriginal(int pos) const
{
	if (pos < 0 || pos >= toOriginal.size())
		return -1;
	return toOriginal[pos];
}

bool StoryTextSnapshot::mapRangeToOriginal(int filteredStart, int filteredLength, int& originalStart, int& originalLength) const
{
	if (filteredStart < 0 || filteredStart >= toOriginal.size())
		return false;
	int filteredEnd = filteredStart + filteredLength - 1;
	if (filteredEnd < 0 || filteredEnd >= toOriginal.size())
		return false;
	originalStart = toOriginal[filteredStart];
	originalLength = toOriginal[filteredEnd] - originalStart + 1;
	return true;
}
