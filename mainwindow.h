#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStack>

class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// ============================================================
// 牌面状态：4 个槽位，每个槽位三张信息
//   value[i] 第 i 张牌的数值
//   expr[i]  第 i 张牌的表达式（显示在算式条上），如 "(8-3)"
//   used[i]  第 i 张牌是否已被用掉
// 整个结构体可以直接赋值拷贝 —— 撤销功能就靠这个
// ============================================================
struct Board {
    int     value[4] = {0, 0, 0, 0};
    QString expr[4];
    bool    used[4]  = {false, false, false, false};
};

// ============================================================
// 主窗口
//
// 每个按钮对应一个槽函数，一目了然：
//
//   数字牌   btnNum0~3  ->  onBtnNum0Clicked()~onBtnNum3Clicked()
//   运算符   btnAdd     ->  onBtnAddClicked()   （+）
//           btnSub     ->  onBtnSubClicked()   （-）
//           btnMul     ->  onBtnMulClicked()   （×）
//           btnDiv     ->  onBtnDivClicked()   （÷）
//   功能键   btnUndo    ->  onBtnUndoClicked()  （撤销一步）
//           btnHint    ->  onBtnHintClicked()  （提示）
//           btnReset   ->  onBtnResetClicked() （重置本局）
//
// 四个数字牌槽函数都只有一行：转调 handleCard(槽位号)
// 四个运算符槽函数都只有一行：转调 handleOp('运算符')
// 真正要写的逻辑集中在 handleCard / handleOp / 各 TODO 函数里
// ============================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // ---- 数字牌（已实现一行转发，不用改）----
    void onBtnNum0Clicked();
    void onBtnNum1Clicked();
    void onBtnNum2Clicked();
    void onBtnNum3Clicked();

    // ---- 运算符（已实现一行转发，不用改）----
    void onBtnAddClicked();
    void onBtnSubClicked();
    void onBtnMulClicked();
    void onBtnDivClicked();

    // ---- 功能按钮（TODO：由你实现）----
    void onBtnUndoClicked();
    void onBtnHintClicked();
    void onBtnResetClicked();

private:
    // ===== 集中写逻辑的地方（TODO：由你实现）=====
    void playMergeAnimation(int slotA,int slotB,QString op);//播淡出动画，播完才合并
    void handleCard(int slot);              // 点了第 slot 张牌（状态机核心）
    void handleOp(QChar op);                // 点了运算符（已写好，作参考）
    void dealCards();                       // 发牌
    void mergeCards(int slotA, int slotB, QString op); // 合并 A op B 放回 slotA
    void checkGameOver();                   // 剩一张牌时判胜负
    bool solve(int nums[], int n, QStringList &steps); // DFS 求解器（提示用）

    // ===== 界面辅助（已实现，不用改）=====
    void refreshUi();                       // 按当前状态刷新所有控件
    void setCardSelected(int slot, bool on); // 给牌加/去金色选中边框
    int  remainingCount() const;            // 还剩几张牌

    Ui::MainWindow *ui;

    // ===== 游戏数据 =====
    // 三个变量组合出游戏的三种状态：
    //   m_first == -1              空闲：还没选牌
    //   m_first >= 0 且 m_op 为空   已选第一张牌，等运算符
    //   m_first >= 0 且 m_op 非空   已选运算符，等第二张牌
    int     m_first = -1;    // 第一张已选牌的槽位，-1 表示未选
    QString m_op;            // 已选运算符 "+" "-" "*" "/"，空串表示未选
    bool m_animating =false;//新增：合并动画播放中（锁住输入）

    Board           m_board;      // 当前牌面
    Board           m_initial;    // 本局初始牌面（重置用）
    QStack<Board>   m_undoStack;  // 撤销栈：每次合并前压入一份快照

    // ===== 提示功能数据 =====
    int         m_hintLevel = 0;  // 提示点了几次：1 暗示 2 第一步 3 完整解法
    QStringList m_hintSteps;      // 当前牌面的一组解，如 {"8-3=5", ...}
};
#endif // MAINWINDOW_H
