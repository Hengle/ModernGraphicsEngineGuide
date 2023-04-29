#include <QApplication>
#include "QRenderWidget.h"
#include "Render/Pass/QBasePassForward.h"

int main(int argc, char** argv) {
	QApplication app(argc, argv);
	QRhiWindow::InitParams initParams;
	QRenderWidget widget(initParams);
	widget.setupCamera();

	widget.showMaximized();
	return app.exec();
}
