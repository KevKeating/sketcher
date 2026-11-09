#pragma once

#include <QFrame>
#include <QLabel>

#include "schrodinger/sketcher/definitions.h"

class QToolButton;

namespace schrodinger
{
namespace sketcher
{

/**
 * How a CustomTitleBar is drawn
 */
enum class TitleBarAppearance {
    // white text on a dark background, which replaces emscripten's title bar
    // on WASM
    DARK,
    // the system palette's text and window colors and the platform style's
    // close icon, which look like the platform's own controls on desktop
    // builds
    NATIVE_LIKE,
};

/**
 * Custom title bar with a fixed height and styled QLabel for the title.
 * The default title bar drawn by emscripten looks really bad so dialogs draw
 * their own on WASM. Popups never have a native title bar, so they can use
 * this on any platform. By default, the user can drag the title bar to move
 * the parent widget. Note: For styling purposes, parent widget's layout needs
 * to have a margin of 0.
 */
class SKETCHER_API CustomTitleBar : public QFrame
{
    Q_OBJECT
  public:
    CustomTitleBar(const QString& title, QWidget* parent = nullptr,
                   TitleBarAppearance appearance = TitleBarAppearance::DARK);
    ~CustomTitleBar();
    QLabel* m_title_label = nullptr;

    /**
     * Add a close button to the right end of the title bar, if there isn't one
     * already. The title bar doesn't connect it to anything.
     *
     * @return The close button
     */
    QToolButton* addCloseButton();

    /**
     * @return The close button, or nullptr if addCloseButton() was never
     * called
     */
    QToolButton* getCloseButton() const;

    /**
     * Set whether dragging the title bar moves the parent widget
     */
    void setDraggable(bool draggable);

  private:
    /**
     * Paints the background of the title bar.
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * Sets the start position of the mouse press event.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * Moves the parent widget to the new position.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    TitleBarAppearance m_appearance;
    QToolButton* m_close_button = nullptr;
    bool m_draggable = true;
    QPointF m_cursor_start_pos;
    bool m_drag_started = false;
};

} // namespace sketcher
} // namespace schrodinger
