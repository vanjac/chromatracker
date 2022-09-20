#include "editor.h"
#include "songops.h"

namespace chromatracker::edit {

void Editor::reset()
{
    undoer.reset(&song);

    // reset selected
    selected.pitch = MIDDLE_C;
    selected.velocity = 1.0f;
    std::shared_lock lock(song.mu);
    if (!song.samples.empty()) {
        selected.sample = song.samples.front();
    } else {
        selected.sample.reset();
    }

    // reset cursor
    editCur.cursor.song = &song;
    editCur.track = 0;
    resetCursor();
}

void Editor::resetCursor()
{
    std::shared_lock lock(song.mu);
    if (!song.sections.empty()) {
        editCur.cursor.section = song.sections.front();
        editCur.cursor.time = 0;
    }
}

void Editor::snapToGrid()
{
    editCur.cursor.time /= cellSize;
    editCur.cursor.time *= cellSize;
}

void Editor::nextCell()
{
    snapToGrid();
    editCur.cursor.time += cellSize;
    ticks sectionLength;
    if (auto sectionP = editCur.cursor.section.lock()) {
        std::shared_lock sectionLock(sectionP->mu);
        sectionLength = sectionP->length;
    } else {
        return;
    }
    if (editCur.cursor.time >= sectionLength) {
        auto next = editCur.cursor.nextSection();
        if (next) {
            editCur.cursor.section = next;
            editCur.cursor.time = 0;
        } else {
            editCur.cursor.time = sectionLength - 1;
            snapToGrid();
        }
    }
}

void Editor::prevCell()
{
    if (editCur.cursor.time % cellSize != 0) {
        snapToGrid();
    } else if (editCur.cursor.time < cellSize) {
        auto prev = editCur.cursor.prevSection();
        if (prev) {
            editCur.cursor.section = prev;
            std::shared_lock sectionLock(prev->mu);
            editCur.cursor.time = prev->length - 1;
            snapToGrid();
        }
    } else {
        editCur.cursor.time -= cellSize;
    }
}

void Editor::select(const Event &event)
{
    selected.merge(event);
    selected.special = Event::Special::None; // don't store
    selected.time = 0;
}

int Editor::selectedSampleIndex()
{
    // indices are easier to work with than iterators in this case
    auto &samples = song.samples;
    auto it = std::find(samples.begin(), samples.end(), selected.sample.lock());
    if (it == samples.end()) {
        return -1;
    } else {
        return it - samples.begin();
    }
}

void Editor::writeEvent(bool playing, const Event &event, Event::Mask mask,
                        bool continuous)
{
    if (!overwrite) {
        undoer.doOp(edit::ops::MergeEvent(
            editCur, event, mask), continuous);
    } else if (record) {
        undoer.doOp(edit::ops::WriteCell(
            editCur, playing ? 1 : cellSize, event), continuous);
        // TODO if playing, clear events
    }
    // TODO combine into single undo operation while playing
}

} // namespace
