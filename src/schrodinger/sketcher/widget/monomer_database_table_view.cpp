#include "schrodinger/sketcher/widget/monomer_database_table_view.h"

#include <QColor>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QStyledItemDelegate>

#include "schrodinger/sketcher/sketcher_css_style.h"

namespace schrodinger
{
namespace sketcher
{

static const QString NO_MATCHES_TEXT =
    "No monomers match search & filter criteria";

namespace
{

/**
 * Paint the current row's background when it isn't selected, and skip Qt's
 * per-cell focus rectangle, since the current row as a whole is highlighted
 */
class CurrentRowDelegate : public QStyledItemDelegate
{
  public:
    CurrentRowDelegate(QTableView* view) :
        QStyledItemDelegate(view),
        m_view(view)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!(option.state & QStyle::State_Selected) &&
            index.row() == m_view->currentIndex().row()) {
            painter->fillRect(option.rect, QColor(HOVER_BACKGROUND_COLOR));
        }
        auto option_without_focus = option;
        option_without_focus.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, option_without_focus, index);
    }

  private:
    QTableView* m_view;
};

} // namespace

MonomerDatabaseTableView::MonomerDatabaseTableView(QWidget* parent) :
    QTableView(parent)
{
    setItemDelegate(new CurrentRowDelegate(this));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setWordWrap(false);
    setShowGrid(false);
    setCornerButtonEnabled(false);
    verticalHeader()->hide();
    // Fixed row heights keep model resets fast for large databases, since only
    // the visible rows need to be laid out. Never use ResizeToContents here.
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setSectionsClickable(true);
}

void MonomerDatabaseTableView::setMutationMode(bool mutation_mode)
{
    m_mutation_mode = mutation_mode;
}

bool MonomerDatabaseTableView::isMutationMode() const
{
    return m_mutation_mode;
}

int MonomerDatabaseTableView::calcHeightForRows(int num_rows) const
{
    return horizontalHeader()->sizeHint().height() +
           num_rows * verticalHeader()->defaultSectionSize() + 2 * frameWidth();
}

QSize MonomerDatabaseTableView::sizeHint() const
{
    return {QTableView::sizeHint().width(),
            calcHeightForRows(PREFERRED_VISIBLE_ROWS)};
}

QSize MonomerDatabaseTableView::minimumSizeHint() const
{
    return {QTableView::minimumSizeHint().width(),
            calcHeightForRows(MIN_VISIBLE_ROWS)};
}

QItemSelectionModel::SelectionFlags
MonomerDatabaseTableView::selectionCommand(const QModelIndex& index,
                                           const QEvent* event) const
{
    if (m_mutation_mode) {
        // Stop the selection at its source, so that the user only ever moves
        // the current index
        return QItemSelectionModel::NoUpdate;
    }
    return QTableView::selectionCommand(index, event);
}

void MonomerDatabaseTableView::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
        // Consume these keys so that Qt doesn't also change the selection or
        // emit activated()
        event->accept();
        if (!event->isAutoRepeat() && currentIndex().isValid()) {
            emit commitRequested(currentIndex().row());
        }
        return;
    }
    QTableView::keyPressEvent(event);
}

void MonomerDatabaseTableView::updateRow(int row)
{
    if (row >= 0) {
        viewport()->update(QRect(0, rowViewportPosition(row),
                                 viewport()->width(), rowHeight(row)));
    }
}

void MonomerDatabaseTableView::currentChanged(const QModelIndex& current,
                                              const QModelIndex& previous)
{
    QTableView::currentChanged(current, previous);
    // The current row highlight spans the whole row, but Qt only repaints the
    // current and previous cells
    updateRow(previous.row());
    updateRow(current.row());
}

void MonomerDatabaseTableView::paintEvent(QPaintEvent* event)
{
    QTableView::paintEvent(event);
    if (model() == nullptr || model()->rowCount() > 0) {
        return;
    }
    QPainter painter(viewport());
    auto font = painter.font();
    font.setItalic(true);
    painter.setFont(font);
    painter.setPen(QColor(INFO_TEXT_COLOR));
    painter.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap,
                     NO_MATCHES_TEXT);
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/widget/monomer_database_table_view.moc"
