#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPushButton>
#include <QStyle>
#include <QRandomGenerator>
#include<QGraphicsOpacityEffect>//透明度特效
#include<QPropertyAnimation>//属性动画

// ============================================================
//  24点游戏 · 代码框架（直白版）
//  ------------------------------------------------------------
//  每个按钮一个槽函数，连接关系全部写在构造函数里。
//
//  不用改的部分：
//    · 构造函数（信号连接 + 开局）
//    · 11 个槽函数本身（数字牌和运算符只有一行转发）
//    · refreshUi / setCardSelected / remainingCount 界面辅助
//
//  由你实现的部分（按编号顺序做，由易到难）：
//    TODO 1  dealCards       发牌
//    TODO 2  handleCard      选牌状态机（核心）
//    TODO 3  mergeCards      合并两张牌
//    TODO 4  onBtnUndoClicked  撤销
//    TODO 5  onBtnResetClicked 重置
//    TODO 6  checkGameOver   胜负判定
//    TODO 7  solve + onBtnHintClicked  求解器与提示
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ========== 信号连接：按钮 -> 槽函数 ==========
    // 数字牌
    connect(ui->btnNum0, &QPushButton::clicked, this, &MainWindow::onBtnNum0Clicked);
    connect(ui->btnNum1, &QPushButton::clicked, this, &MainWindow::onBtnNum1Clicked);
    connect(ui->btnNum2, &QPushButton::clicked, this, &MainWindow::onBtnNum2Clicked);
    connect(ui->btnNum3, &QPushButton::clicked, this, &MainWindow::onBtnNum3Clicked);
    // 运算符
    connect(ui->btnAdd, &QPushButton::clicked, this, &MainWindow::onBtnAddClicked);
    connect(ui->btnSub, &QPushButton::clicked, this, &MainWindow::onBtnSubClicked);
    connect(ui->btnMul, &QPushButton::clicked, this, &MainWindow::onBtnMulClicked);
    connect(ui->btnDiv, &QPushButton::clicked, this, &MainWindow::onBtnDivClicked);
    // 功能键
    connect(ui->btnUndo,  &QPushButton::clicked, this, &MainWindow::onBtnUndoClicked);
    connect(ui->btnHint,  &QPushButton::clicked, this, &MainWindow::onBtnHintClicked);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onBtnResetClicked);

    // 开局
    dealCards();
    refreshUi();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ============================================================
//  数字牌槽函数：只负责报出自己的槽位号，逻辑在 handleCard 里
// ============================================================
void MainWindow::onBtnNum0Clicked() { handleCard(0); }
void MainWindow::onBtnNum1Clicked() { handleCard(1); }
void MainWindow::onBtnNum2Clicked() { handleCard(2); }
void MainWindow::onBtnNum3Clicked() { handleCard(3); }

// ============================================================
//  运算符槽函数：只负责报出自己是哪个运算符
// ============================================================
void MainWindow::onBtnAddClicked() { handleOp('+'); }
void MainWindow::onBtnSubClicked() { handleOp('-'); }
void MainWindow::onBtnMulClicked() { handleOp('*'); }
void MainWindow::onBtnDivClicked() { handleOp('/'); }

// ============================================================
//  TODO 1：发牌
//  现在写死成 {8, 5, 2, 3} 方便调试（有解，比如 8×(5−2)×... 自己算）
//  等 TODO 7 的 solve() 写完后改成随机发牌 + 无解重发：
//      do {
//          4 个数都用 QRandomGenerator::global()->bounded(1, 11) 生成
//      } while (!solve(...));
// ============================================================
void MainWindow::dealCards()
{
    int nums[4];
    QStringList tmp;
    while(true){
        for(int i=0;i<4;i++)
            nums[i]=QRandomGenerator::global()->bounded(1,14);//随机数1~13
        tmp.clear();
        if(solve(nums,4,tmp))
            break;//有解才停，无解重新随机
    }
    for (int i = 0; i < 4; i++) {
        m_board.value[i] = nums[i];
        m_board.expr[i]  = QString::number(nums[i]); // 初始牌的表达式就是数字本身
        m_board.used[i]  = false;
    }

    m_initial   = m_board;   // 记住初始牌面，重置按钮用
    m_hintLevel = 0;
    m_hintSteps.clear();
}

// ============================================================
//  TODO 2：选牌状态机（核心）
//
//  三个变量表示三种状态（见 .h 注释）：
//    m_first == -1              → 空闲
//    m_first >= 0 且 m_op.isEmpty() → 已选第一张牌
//    m_first >= 0 且 m_op 非空      → 已选运算符
//
//  按这个流程写：
//    1) 如果这张牌已用掉 m_board.used[slot]，直接 return
//    2) 空闲状态：选中它 → m_first = slot
//    3) 已选第一张牌状态：
//         点的还是同一张 → 取消选择：m_first = -1
//         点的是另一张   → 换选：m_first = slot
//    4) 已选运算符状态：
//         点的还是同一张 → 全部取消：m_first = -1; m_op.clear();
//         点的是另一张   → 它是第二张牌：
//             若 m_op=="/"：除数为 0 或除不尽 → 提示「除不尽，换一个」并 return
//             否则 mergeCards(m_first, slot, m_op);
//                  m_first = -1; m_op.clear();   // 回到空闲
//                  checkGameOver();
//    5) 最后调用 refreshUi() 刷新界面
// ============================================================
void MainWindow::handleCard(int slot)
{
    //用掉的牌不响应
    if(m_board.used[slot])
        return;

    //空闲状态：选中这张牌
    if(m_first == -1){
        m_first = slot;
    }else if(m_op.isEmpty()){
        //已选第一张牌，还没选运算符
        if(slot == m_first)
            m_first = -1;   //点同一张=取消
        else
            m_first = slot;//点另一张=换选
    }else{
        //已选运算符，现在点的是第二张牌
        if(slot == m_first){
            //点的还是同一张 = 全部取消，回空闲
            m_first = -1;
            m_op.clear();
        }else{
            //除法规则：除数为0或除不尽，拒绝并提示
            if(m_op == "/" && (m_board.value[slot] == 0 || m_board.value[m_first] % m_board.value[slot] != 0)){
                ui->labelStatus->setText(QStringLiteral("除不尽，换一个"));
                return;//直接返回，不走refreshUi——否则提示文字会被它覆盖
            }
            m_animating=true;//锁输入
            int slotA=m_first,slotB=slot;//先把槽位存下来
            QString op=m_op;
            m_first=-1;//清空选择
            m_op.clear();//回到空闲
            refreshUi();//刷新（此时按钮全锁）
            playMergeAnimation(slotA,slotB,op);//播动画，播完它自己调用mergeCards
            return;
        }
    }
    refreshUi();//通知界面刷新（框架写好的）
    checkGameOver();//TODO6结束游戏
}

// ============================================================
//  点运算符（已写好，作为槽函数写法的参考样例）
//  规则：没选第一张牌时运算符是灰的点不到；已选运算符后再点
//  别的运算符 = 直接换（定稿决议）
// ============================================================
void MainWindow::handleOp(QChar op)
{
    if (m_first == -1)
        return;                 // 空闲状态不响应（双保险）

    m_op = QString(op);         // 记下运算符（重复点就是换）
    refreshUi();
}

// ============================================================
//  TODO 3：合并两张牌 —— A op B，结果放回 slotA 的位置
//  步骤：
//    1) m_undoStack.push(m_board);        // 先存快照，才能撤销
//    2) 按 op 算结果（/ 已保证整除，直接用整数除法）
//    3) 拼子表达式：
//       m_board.expr[slotA] = "(" + exprA + op + exprB + ")";
//    4) m_board.value[slotA] = 结果;
//    5) m_board.used[slotB]  = true;      // B 被用掉
// ============================================================
void MainWindow::mergeCards(int slotA, int slotB, QString op)
{
    //合并前把当前牌面整个拍照，压入撤销栈
    m_undoStack.push(m_board);

    //按运算符算结果
    int a = m_board.value[slotA];
    int b = m_board.value[slotB];
    int result = 0;
    if(op == "+")
        result = a + b;
    else if(op == "-")
        result = a - b;
    else if(op == "*")
        result = a * b;
    else if(op == "/")
        result = a / b; //入口处已保证整除

    //新牌占据slotA的位置：更新数值和表达式
    m_board.value[slotA] = result;
    m_board.expr[slotA] = "(" + m_board.expr[slotA] + " " + op + " " + m_board.expr[slotB] + ")";

    //slotB 被用掉（refreshUi 会把它隐藏）
    m_board.used[slotB] = true;

    //算式条上展示刚合并出来的表达式
    ui->lineEditDisplay->setText(m_board.expr[slotA]);
}


void MainWindow::playMergeAnimation(int slotA, int slotB, QString op)
{
    QPushButton *btns[4] = {ui->btnNum0, ui->btnNum1, ui->btnNum2, ui->btnNum3};

    // ① 给 B 牌挂一个透明度特效
    auto *effect = new QGraphicsOpacityEffect(btns[slotB]);
    btns[slotB]->setGraphicsEffect(effect);

    // ② 动画：250 毫秒内 opacity 从 1.0（不透明）变到 0.0（全透明）
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(250);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);

    // ③ 动画播完 → 真正合并。这里是 lambda：就地写一个小函数当回调
    connect(anim, &QPropertyAnimation::finished, this, [=] {
        btns[slotB]->setGraphicsEffect(nullptr);  // 摘掉特效（不然下次还是透明的）
        mergeCards(slotA, slotB, op);             // 真正的合并
        m_animating = false;                      // 解锁输入
        refreshUi();
        checkGameOver();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped); // 播完自动销毁动画对象
}

// ============================================================
//  TODO 4：撤销一步
//  步骤：
//    1) 栈空就 return
//    2) m_board = m_undoStack.pop();      // 恢复快照
//    3) 清空选择：m_first = -1; m_op.clear();
//    4) 提示状态作废：m_hintLevel = 0; m_hintSteps.clear();
//    5) refreshUi();
// ============================================================
void MainWindow::onBtnUndoClicked()
{
    if(m_undoStack.isEmpty())
        return;//栈空，没得撤

    m_board = m_undoStack.pop();//恢复上一次合并前的快照

    m_first = -1;//清空选择状态

    m_op.clear();
    m_hintLevel = 0;//牌面变了，提示作废

    m_hintSteps.clear();

    ui->lineEditDisplay->clear();//清空算式条
    refreshUi();
}

// ============================================================
//  TODO 5：重置本局（恢复初始牌面）
//  步骤：
//    1) m_board = m_initial;
//    2) m_undoStack.clear();
//    3) 清空选择、提示状态清零
//    4) refreshUi();
// ============================================================
void MainWindow::onBtnResetClicked()
{
    m_board = m_initial;//恢复本局最开始的牌面

    m_undoStack.clear();//清空撤销历史

    m_first = -1;
    m_op.clear();
    m_hintLevel = 0;
    m_hintSteps.clear();

    ui->lineEditDisplay->clear();//清空算式条
    refreshUi();
}

// ============================================================
//  TODO 6：胜负判定（每次合并后调用）
//  步骤：
//    1) remainingCount() != 1 就 return（还没算完）
//    2) 找到那张没用掉的牌：
//         数值 == 24 → ui->labelStatus 显示「🎉 24！你赢了」
//         否则       → 「最后一张是 X，差一点，点撤销回去试试」
// ============================================================
void MainWindow::checkGameOver()
{
    if(remainingCount() != 1)
        return;//还没算完，不判定

    //找到那一张唯一没用掉的牌
    int last = 0;
    for(int i=0;i<4;i++){
        if(!m_board.used[i])
            last = m_board.value[i];
    }

    if(last == 24)
        ui->labelStatus->setText(QStringLiteral("恭喜你！！答对了！！点重置再来一局吧！"));
    else{
            ui->labelStatus->setText(QStringLiteral("最后一张牌是%1，就差一步，点撤销回去试试？？").arg(last));
    }
}

// ============================================================
//  TODO 7：DFS 求解器 + 渐进式提示
//
//  solve(nums, n, steps)：判断 n 个数能否算出 24，能则把步骤
//  写进 steps 并返回 true。经典 DFS 思路：
//    · n == 1 时：return nums[0] == 24;
//    · 否则枚举两张牌 (i, j)（i < j）和四种运算符：
//        - 算出结果 v，记录一步文字如 "8-3=5"
//        - 把剩余数字和 v 组成新数组，递归 solve
//        - 递归返回 true：steps.append(这一步); return true;
//    · 全部试过都不行：return false;
//  注意：减法 a-b 和 b-a 都算（顺序有意义）；
//        除法只在能整除时尝试（两个方向也都要试）。
//
//  onBtnHintClicked()：连点升级（蓝本 3.7）
//    m_hintLevel++;
//    第 1 次：先 solve 当前牌面存进 m_hintSteps（只解一次），
//             labelStatus 暗示「试试先动 X 和 Y」
//    第 2 次：显示第一步 m_hintSteps[0]
//    第 3 次：显示完整解法 m_hintSteps.join("，")
// ============================================================
bool MainWindow::solve(int nums[], int n, QStringList &steps)
{
    //递归出口：只剩一个数
    if(n == 1)
        return nums[0] == 24;

    //枚举两张牌（i<j 避免重复选同一对）
    for(int i=0;i<n;i++){
        for(int j=i+1;j<n;j++){
            int a=nums[i];
            int b=nums[j];

            //这两张牌的六种可能运算（加法乘法不分顺序，减法除法两个方向都要试）
            int vals[6], cnt=0;
            QString desc[6];

            vals[cnt]=a+b;
            desc[cnt++]=QString("%1+%2=%3").arg(a).arg(b).arg(a+b);

            vals[cnt]=a*b;
            desc[cnt++]=QString("%1x%2=%3").arg(a).arg(b).arg(a*b);

            vals[cnt]=a-b;
            desc[cnt++]=QString("%1-%2=%3").arg(a).arg(b).arg(a-b);

            vals[cnt]=b-a;
            desc[cnt++]=QString("%1-%2=%3").arg(b).arg(a).arg(b-a);

            if(b!=0 && a%b==0){
                vals[cnt]=a/b;
                desc[cnt++]=QString("%1÷%2=%3").arg(a).arg(b).arg(a/b);
            }
            if(a!=0 && b%a==0){
                vals[cnt]=b/a;
                desc[cnt++]=QString("%1÷%2=%3").arg(b).arg(a).arg(b/a);
            }

            //每种运算都递归试一次
            for(int k=0;k<cnt;k++){
                int rest[4],m=0;
                for(int t=0;t<n;t++)
                    if(t!=i && t!=j)
                        rest[m++]=nums[t];
                rest[m++]=vals[k];//结果作为新的一张“牌”

                //递归：n-1个数能不能算出24？
                if(solve(rest,n-1,steps)){
                    steps.prepend(desc[k]);//成功路径上记下这一步
                    return true;
                }
            }
        }
    }
    return false;
}

void MainWindow::onBtnHintClicked()
{
    //只在第一次点时求解（牌面没变就别重复算）
    if(m_hintSteps.isEmpty()){
        int nums[4],n=0;
        for(int i=0;i<4;i++)
            if(!m_board.used[i])
                nums[n++]=m_board.value[i];

        if(!solve(nums,n,m_hintSteps)){
            ui->labelStatus->setText(QStringLiteral("当前牌面无解，点重置换一局吧！！"));
            return;
        }
        m_hintLevel=0;
    }

    //渐进式：每点一次多透露一步，全部透露完给完整解法
    if(m_hintLevel<m_hintSteps.size()){
        ui->labelStatus->setText(QStringLiteral("第%1步：%2").arg(m_hintLevel+1).arg(m_hintSteps[m_hintLevel]));
        m_hintLevel++;
    }else{
        ui->labelStatus->setText(QStringLiteral("完整解法：%1").arg(m_hintSteps.join("，")));
    }
}

// ============================================================
//  以下界面辅助函数已实现，不用改
// ============================================================

int MainWindow::remainingCount() const
{
    int n = 0;
    for (int i = 0; i < 4; ++i)
        if (!m_board.used[i]) ++n;
    return n;
}

void MainWindow::setCardSelected(int slot, bool on)
{
    QPushButton *btns[4] = {ui->btnNum0, ui->btnNum1, ui->btnNum2, ui->btnNum3};
    btns[slot]->setProperty("selected", on);  // 命中 QSS 里的金边样式
    btns[slot]->style()->unpolish(btns[slot]);
    btns[slot]->style()->polish(btns[slot]);
}

// 按当前状态刷新全部控件，所有界面反馈集中在这一个函数里
void MainWindow::refreshUi()
{
    // ---- 数字牌：文字 / 可见性 / 选中金边 ----
    QPushButton *btns[4] = {ui->btnNum0, ui->btnNum1, ui->btnNum2, ui->btnNum3};
    for (int i = 0; i < 4; ++i) {
        if (m_board.used[i]) {
            btns[i]->hide();                 // 用掉的牌隐藏空槽
            btns[i]->setEnabled(!m_animating);
            continue;
        }
        btns[i]->show();
        btns[i]->setText(QString::number(m_board.value[i]));
        setCardSelected(i, i == m_first);    // 只有 m_first 那张带金边
    }

    // ---- 运算符：选了第一张牌才亮起 ----
    bool opEnabled = (m_first != -1) && !m_animating;
    ui->btnAdd->setEnabled(opEnabled);
    ui->btnSub->setEnabled(opEnabled);
    ui->btnMul->setEnabled(opEnabled);
    ui->btnDiv->setEnabled(opEnabled);

    // ---- 撤销：栈空时置灰 ----
    ui->btnUndo->setEnabled(!m_undoStack.isEmpty() && !m_animating);
    ui->btnHint->setEnabled(!m_animating);
    ui->btnReset->setEnabled(!m_animating);

    // ---- 算式显示条与状态提示 ----
    if (m_first == -1) {
        // 空闲
        ui->labelStatus->setText(QStringLiteral("先选一张牌，再选运算符"));
    } else if (m_op.isEmpty()) {
        // 已选第一张牌
        ui->lineEditDisplay->setText(m_board.expr[m_first]);
        ui->labelStatus->setText(
            QStringLiteral("已选 %1，再选一个运算符").arg(m_board.value[m_first]));
    } else {
        // 已选运算符，等第二张牌
        ui->lineEditDisplay->setText(m_board.expr[m_first] + " " + m_op);
        ui->labelStatus->setText(
            QStringLiteral("%1 %2 ?  再选一张牌")
                .arg(m_board.value[m_first]).arg(m_op));
    }
}
