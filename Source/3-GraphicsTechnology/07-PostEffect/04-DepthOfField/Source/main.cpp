#include "QEngineApplication.h"
#include "QRenderWidget.h"
#include "Render/QFrameGraph.h"
#include "Render/Component/QStaticMeshRenderComponent.h"
#include "Render/Pass/QDilationRenderPass.h"
#include "Render/Pass/QDepthOfFieldRenderPass.h"
#include "Render/Pass/PBR/QPbrBasePassDeferred.h"
#include "Render/Component/QParticlesRenderComponent.h"
#include <QPainter>
#include <QPainterPath>

class MyGpuParticleEmitter : public QGpuParticleEmitter {
public:
	MyGpuParticleEmitter() {
		QGpuParticleEmitter::InitParams params;
		params.spawnParams->addParam("MinSize", 0.0f);
		params.spawnParams->addParam("MaxSize", 0.01f);

		params.spawnDefine = R"(	
			float rand(float seed, float min, float max){
				return min + (max-min) * fract(sin(dot(vec2(gl_GlobalInvocationID.x * seed * UpdateCtx.deltaSec,UpdateCtx.timestamp) ,vec2(12.9898,78.233))) * 43758.5453);
			}
		)";

		params.spawnCode = R"(		
			outParticle.age = 0.0f;
			outParticle.lifetime = 3.0f;
			outParticle.scaling = vec3(rand(0.45,Params.MinSize,Params.MaxSize));
			
			const float noiseStrength = 10;
			vec3 noiseOffset = vec3(rand(0.12,-noiseStrength,noiseStrength),0,rand(0.11561,-noiseStrength,noiseStrength));
			outParticle.position = noiseOffset;
			outParticle.velocity = vec3(0,rand(0.124,0,0.0005),rand(0.4451,-0.0005,0.0005));
		)";

		params.updateDefine = R"(	
			float rand(float seed, float min, float max){
				return min + (max-min) * fract(sin(dot(vec2(gl_GlobalInvocationID.x * seed * UpdateCtx.deltaSec,UpdateCtx.timestamp) ,vec2(12.9898,78.233))) * 43758.5453);
			}
		)";

		params.updateCode = R"(		
			outParticle.age	 = inParticle.age + UpdateCtx.deltaSec;
			outParticle.position = inParticle.position + inParticle.velocity;
			outParticle.velocity = inParticle.velocity + vec3(rand(0.41,-0.000001,0.000001),0,0);
			outParticle.scaling  = inParticle.scaling;
			outParticle.rotation = inParticle.rotation;
		)";
		setupParams(params);
		mNumOfSpawnPerFrame = 1000;
	}
	void onUpdateAndRecyle(QRhiCommandBuffer* inCmdBuffer) override {
		QGpuParticleEmitter::onUpdateAndRecyle(inCmdBuffer);
	}
};

int main(int argc, char **argv){
	QEngineApplication app(argc, argv);
	QLoggingCategory::setFilterRules("qt.vulkan=true");
	QRhiWindow::InitParams initParams;
	initParams.backend = QRhi::Implementation::Vulkan;
	QRenderWidget widget(initParams);

	auto camera = widget.setupCamera();
	camera->setPosition(QVector3D(0.0f, 0.1f, 10.0f));

	QImage image(100,100,QImage::Format_RGBA8888);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	QPoint center(50, 50);
	QPainterPath path;
	for (int i = 0; i < 5; i++) {
		float x1 = qCos((18 + 72 * i) * M_PI/180) * 50,
			  y1 = qSin((18 + 72 * i) * M_PI/180) * 50,
			  x2 = qCos((54 + 72 * i) * M_PI/180) * 20,
			  y2 = qSin((54 + 72 * i) * M_PI/180) * 20;

		if (i == 0) {
			path.moveTo(x1 + center.x(), y1 + center.y());
			path.lineTo(x2 + center.x(), y2 + center.y());
		}
		else {
			path.lineTo(x1 + center.x(), y1 + center.y());
			path.lineTo(x2 + center.x(), y2 + center.y());
		}
		
	}
	path.closeSubpath();
	painter.fillPath(path,Qt::white);

	widget.setFrameGraph(
		QFrameGraph::Begin()
		.addPass(
			QPbrBasePassDeferred::Create("BasePass")
			.addComponent(
				QParticlesRenderComponent::Create("Particles")
				.setEmitter(new MyGpuParticleEmitter)
				.setParticleShape(QStaticMesh::CreateFromImage(image))
			)
		)
		.addPass(
			QDepthOfFieldRenderPass::Create("DepthOfField")
			.setTextureIn_Src("BasePass", QPbrBasePassDeferred::Out::BaseColor)
			.setTextureIn_Position("BasePass", QPbrBasePassDeferred::Out::Position)
		)
		.end("DepthOfField", QDepthOfFieldRenderPass::Result)
	);

	widget.showMaximized();
	return app.exec();
}
