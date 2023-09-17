#include <QApplication>

#include "Render/RHI/QRhiWindow.h"
#include "Render/Painter/TexturePainter.h"

class ComputeShaderWindow : public QRhiWindow {
private:
	QRhiSignal mSigInit;
	QRhiSignal mSigSubmit;

	QScopedPointer<QRhiBuffer> mStorageBuffer;
	QScopedPointer<QRhiTexture> mTexture;

	QScopedPointer<QRhiComputePipeline> mPipeline;
	QScopedPointer<QRhiShaderResourceBindings> mShaderBindings;

	QScopedPointer<TexturePainter> mTexturePainter;

	const int ImageWidth = 64;
	const int ImageHeight = 64;
public:
	ComputeShaderWindow(QRhiWindow::InitParams inInitParams) :QRhiWindow(inInitParams) {
		mSigInit.request();
		mSigSubmit.request();
	}
protected:
	virtual void onRenderTick() override {
		QRhiRenderTarget* currentRenderTarget = mSwapChain->currentFrameRenderTarget();
		QRhiCommandBuffer* cmdBuffer = mSwapChain->currentFrameCommandBuffer();

		if (mSigInit.ensure()) {
			mStorageBuffer.reset(mRhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, sizeof(float)));							//������������StorageBuffer
			mStorageBuffer->create();
			mTexture.reset(mRhi->newTexture(QRhiTexture::RGBA8, QSize(ImageWidth, ImageHeight), 1, QRhiTexture::UsedWithLoadStore));		//ͼ��ɱ�������߶�ȡ�ʹ洢
			mTexture->create();

			mTexturePainter.reset(new TexturePainter);
			mTexturePainter->setupRhi(mRhi.get());
			mTexturePainter->setupRenderPassDesc(mSwapChain->renderPassDescriptor());
			mTexturePainter->setupSampleCount(mSwapChain->sampleCount());
			mTexturePainter->setupTexture(mTexture.get());
			mTexturePainter->compile();

			mPipeline.reset(mRhi->newComputePipeline());
			mShaderBindings.reset(mRhi->newShaderResourceBindings());
			mShaderBindings->setBindings({
				QRhiShaderResourceBinding::bufferLoadStore(0,QRhiShaderResourceBinding::ComputeStage,mStorageBuffer.get()),					//���ü�����ߵ���Դ�󶨣�Load����ɶ���Store�����д
				QRhiShaderResourceBinding::imageLoadStore(1,QRhiShaderResourceBinding::ComputeStage,mTexture.get(),0),
			});
			mShaderBindings->create();

			QShader cs = QRhiHelper::newShaderFromCode(mRhi.get(), QShader::ComputeStage, R"(#version 440
				layout(std140, binding = 0) buffer StorageBuffer{
					int counter;
				}SSBO;
				layout (binding = 1, rgba8) uniform image2D Tex;
				const int ImageSize = 64 * 64;
				void main(){
					//int currentCounter = SSBO.counter = SSBO.counter + 1;		
					int currentCounter = atomicAdd(SSBO.counter,1);			//use atomic operation
					ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
					imageStore(Tex,pos,vec4(currentCounter/float(ImageSize),0,0,1));
				}
			)");
			Q_ASSERT(cs.isValid());

			mPipeline->setShaderStage({
				QRhiShaderStage(QRhiShaderStage::Compute, cs),
			});

			mPipeline->setShaderResourceBindings(mShaderBindings.get());
			mPipeline->create();
		}
		QRhiResourceUpdateBatch* resourceUpdates = nullptr;
		resourceUpdates = mRhi->nextResourceUpdateBatch();
		const int counter = 0;
		resourceUpdates->uploadStaticBuffer(mStorageBuffer.get(), &counter);
		cmdBuffer->beginComputePass(resourceUpdates);
		cmdBuffer->setComputePipeline(mPipeline.get());
		cmdBuffer->setShaderResources();
		cmdBuffer->dispatch(ImageWidth, ImageHeight, 1);		//����ͼ���С���ֹ�����
		cmdBuffer->endComputePass();

		const QColor clearColor = QColor::fromRgbF(0.2f, 0.2f, 0.2f, 1.0f);
		const QRhiDepthStencilClearValue dsClearValue = { 1.0f,0 };

		cmdBuffer->beginPass(currentRenderTarget, clearColor, dsClearValue);
		mTexturePainter->paint(cmdBuffer, currentRenderTarget);
		cmdBuffer->endPass();
	}
};

int main(int argc, char **argv)
{
    qputenv("QSG_INFO", "1");
    QApplication app(argc, argv);
    QRhiWindow::InitParams initParams;
    ComputeShaderWindow window(initParams);
	window.resize({ 800,600 });
	window.show();
    app.exec();
    return 0;
}