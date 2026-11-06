#include "schrodinger/sketcher/dialog/load_monomer_database.h"

#include <exception>

#include <QByteArray>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "schrodinger/rdkit_extensions/monomer_database.h"
#include "schrodinger/sketcher/dialog/message_box_dialog.h"

namespace schrodinger
{
namespace sketcher
{

void load_monomer_database_from_file(QWidget* parent)
{
    // The parent may be destroyed before the user chooses a file
    QPointer<QWidget> guarded_parent = parent;
    auto file_open_completed = [guarded_parent](const QString& file_path,
                                                const QByteArray& content) {
        if (file_path.isEmpty()) {
            return;
        }
        try {
            auto& db = rdkit_extensions::MonomerDatabase::instance();
            auto result = db.loadMonomersFromJson(content.toStdString());
            if (!result.second.empty()) {
                QStringList failures;
                for (const auto& failure : result.second) {
                    failures.append(QString::fromStdString(failure));
                }
                show_error_dialog("Monomer Database Error", failures.join("\n"),
                                  guarded_parent);
            } else {
                show_information_dialog("Monomer Database",
                                        QFileInfo(file_path).fileName() +
                                            " read successfully.",
                                        guarded_parent);
            }
        } catch (const std::exception& exc) {
            show_error_dialog("Monomer Database Error", exc.what(),
                              guarded_parent);
        }
    };
    QFileDialog::getOpenFileContent("JSON files (*.json)", file_open_completed,
                                    parent);
}

} // namespace sketcher
} // namespace schrodinger
