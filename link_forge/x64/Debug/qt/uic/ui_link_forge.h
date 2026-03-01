/********************************************************************************
** Form generated from reading UI file 'link_forge.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LINK_FORGE_H
#define UI_LINK_FORGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_link_forgeClass
{
public:

    void setupUi(QWidget *link_forgeClass)
    {
        if (link_forgeClass->objectName().isEmpty())
            link_forgeClass->setObjectName("link_forgeClass");
        link_forgeClass->resize(600, 400);

        retranslateUi(link_forgeClass);

        QMetaObject::connectSlotsByName(link_forgeClass);
    } // setupUi

    void retranslateUi(QWidget *link_forgeClass)
    {
        link_forgeClass->setWindowTitle(QCoreApplication::translate("link_forgeClass", "link_forge", nullptr));
    } // retranslateUi

};

namespace Ui {
    class link_forgeClass: public Ui_link_forgeClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LINK_FORGE_H
