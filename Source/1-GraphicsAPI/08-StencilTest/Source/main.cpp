#include <QApplication>
#include <QTime>
#include "QEngineApplication.h"
#include "Render/RHI/QRhiWindow.h"

static QVector2D Vertices[3] = {
	{ 0.0f,    0.5f},
	{-0.5f,   -0.5f},
	{ 0.5f,   -0.5f},
};

struct UniformBlock {
	QGenericMatrix<4, 4, float> MVP;
};

class TriangleWindow : public QRhiWindow {
private:
	QRhiSignal mSigInit;
	QRhiSignal mSigSubmit;

	QScopedPointer<QRhiBuffer> mMaskVertexBuffer;
	QScopedPointer<QRhiGraphicsPipeline> mMaskPipeline;

	QScopedPointer<QRhiBuffer> mVertexBuffer;
	QScopedPointer<QRhiShaderResourceBindings> mShaderBindings;
	QScopedPointer<QRhiGraphicsPipeline> mPipeline;
public:
	TriangleWindow(QRhiHelper::InitParams inInitParams)
		: QRhiWindow(inInitParams) {
		mSigInit.request();
		mSigSubmit.request();
	}
protected:
	virtual void onRenderTick() override {
		if (mSigInit.ensure()) {	
			mMaskVertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(Vertices)));
			mMaskVertexBuffer->create();

			mVertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(Vertices)));
			mVertexBuffer->create();

			QRhiVertexInputLayout inputLayout;
			inputLayout.setBindings({
				QRhiVertexInputBinding(sizeof(QVector2D))
			});

			inputLayout.setAttributes({
				QRhiVertexInputAttribute(0, 0 , QRhiVertexInputAttribute::Float2, 0),
			});

			QShader vs = QRhiHelper::newShaderFromCode(QShader::VertexStage, R"(#version 440
				layout(location = 0) in vec2 position;		
				out gl_PerVertex { 
					vec4 gl_Position;
				};
				void main(){
					gl_Position = vec4(position,0.0f,1.0f);
				}
			)");
			Q_ASSERT(vs.isValid());

			QShader fs = QRhiHelper::newShaderFromCode(QShader::FragmentStage, R"(#version 440	
				layout (location = 0) out vec4 fragColor;	
				void main(){
					fragColor = vec4(1);
				}
			)");
			Q_ASSERT(fs.isValid());

			mShaderBindings.reset(mRhi->newShaderResourceBindings());
			mShaderBindings->create();

			mMaskPipeline.reset(mRhi->newGraphicsPipeline());
			mMaskPipeline->setVertexInputLayout(inputLayout);
			mMaskPipeline->setShaderResourceBindings(mShaderBindings.get());
			mMaskPipeline->setSampleCount(mSwapChain->sampleCount());
			mMaskPipeline->setRenderPassDescriptor(mSwapChainPassDesc.get());
			mMaskPipeline->setShaderStages({
				QRhiShaderStage(QRhiShaderStage::Vertex, vs),
				QRhiShaderStage(QRhiShaderStage::Fragment, fs)
			});
			mMaskPipeline->setFlags(QRhiGraphicsPipeline::Flag::UsesStencilRef);			//������ˮ�ߵ�ģ����Թ���
			mMaskPipeline->setStencilTest(true);											//����ģ�����
			QRhiGraphicsPipeline::StencilOpState maskStencilState;
			maskStencilState.compareOp = QRhiGraphicsPipeline::CompareOp::Never;			//�ù������ڻ���ģ�棨���֣������ǲ�ϣ������RT�ϻ����κ�Ƭ����ɫ�����������Ƭ����Զ����ͨ��ģ�����
			maskStencilState.failOp = QRhiGraphicsPipeline::StencilOp::Replace;				//���õ���Ƭ��û��ͨ��ģ�����ʱ��ʹ��StencilRef���ģ�滺����
			mMaskPipeline->setStencilFront(maskStencilState);								//ָ�������ģ�����
			mMaskPipeline->setStencilBack(maskStencilState);								//ָ�������ģ�����
			mMaskPipeline->create();

			mPipeline.reset(mRhi->newGraphicsPipeline());
			mPipeline->setVertexInputLayout(inputLayout);
			mPipeline->setShaderResourceBindings(mShaderBindings.get());
			mPipeline->setSampleCount(mSwapChain->sampleCount());
			mPipeline->setRenderPassDescriptor(mSwapChainPassDesc.get());
			mPipeline->setShaderStages({
				QRhiShaderStage(QRhiShaderStage::Vertex, vs),
				QRhiShaderStage(QRhiShaderStage::Fragment, fs)
			});
			mPipeline->setFlags(QRhiGraphicsPipeline::Flag::UsesStencilRef);				//������ˮ�ߵ�ģ����Թ���
			mPipeline->setStencilTest(true);												//����ģ�����
			QRhiGraphicsPipeline::StencilOpState stencilState;
			stencilState.compareOp = QRhiGraphicsPipeline::CompareOp::Equal;				//����ϣ���ڵ�ǰ���ߵ�StencilRef����ģ�滺�����ϵ�Ƭ��ֵʱ��ͨ��ģ�����
			stencilState.passOp = QRhiGraphicsPipeline::StencilOp::Keep;					//��ͨ�����Ժ󲻻��ģ�滺�������и�ֵ
			stencilState.failOp = QRhiGraphicsPipeline::StencilOp::Keep;					//��û��ͨ������ʱҲ�����ģ�滺�������и�ֵ
			mPipeline->setStencilFront(stencilState);
			mPipeline->setStencilBack(stencilState);
			mPipeline->create();

		}

		QRhiRenderTarget* renderTarget = mSwapChain->currentFrameRenderTarget();
		QRhiCommandBuffer* cmdBuffer = mSwapChain->currentFrameCommandBuffer();

		if (mSigSubmit.ensure()) {
			QRhiResourceUpdateBatch* batch = mRhi->nextResourceUpdateBatch();
			batch->uploadStaticBuffer(mMaskVertexBuffer.get(), Vertices);					//�ϴ�ģ�棨���֣��Ķ������ݣ�����һ��������

			for (auto& vertex : Vertices) 													//�ϴ�ʵ��ͼ�εĶ������ݣ�����һ�����·�ת��������
				vertex.setY(-vertex.y());
			batch->uploadStaticBuffer(mVertexBuffer.get(), Vertices);

			cmdBuffer->resourceUpdate(batch);
		}

		const QColor clearColor = QColor::fromRgbF(0.0f, 0.0f, 0.0f, 1.0f);
		const QRhiDepthStencilClearValue dsClearValue = { 1.0f,0 };							//ʹ�� 0 ����ģ�滺����
		cmdBuffer->beginPass(renderTarget, clearColor, dsClearValue, nullptr);

		cmdBuffer->setGraphicsPipeline(mMaskPipeline.get());
		cmdBuffer->setStencilRef(1);														//����StencilRefΪ1���ù��߻���ģ�滺���������һ��ֵΪ1������������
		cmdBuffer->setViewport(QRhiViewport(0, 0, mSwapChain->currentPixelSize().width(), mSwapChain->currentPixelSize().height()));
		cmdBuffer->setShaderResources();
		const QRhiCommandBuffer::VertexInput maskVertexInput(mMaskVertexBuffer.get(), 0);
		cmdBuffer->setVertexInput(0, 1, &maskVertexInput);
		cmdBuffer->draw(3);

		cmdBuffer->setGraphicsPipeline(mPipeline.get());
		cmdBuffer->setStencilRef(1);														//����StencilRefΪ1���ù��߻���1����Ӧλ�õ�Ƭ��ģ��ֵ���бȽϣ����ʱ�Ż�ͨ��ģ����ԣ�Ҳ���ǻὫƬ�λ��Ƶ���ɫ������
		cmdBuffer->setViewport(QRhiViewport(0, 0, mSwapChain->currentPixelSize().width(), mSwapChain->currentPixelSize().height()));
		cmdBuffer->setShaderResources();
		const QRhiCommandBuffer::VertexInput vertexInput(mVertexBuffer.get(), 0);
		cmdBuffer->setVertexInput(0, 1, &vertexInput);
		cmdBuffer->draw(3);

		cmdBuffer->endPass();
	}
};

int main(int argc, char** argv)
{
	QEngineApplication app(argc, argv);
	QRhiHelper::InitParams initParams;
	initParams.backend = QRhi::Vulkan;
	TriangleWindow window(initParams);
	window.resize({ 800,600 });
	window.show();
	app.exec();
	return 0;
}