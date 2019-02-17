# consai2_examples

consai2のサンプルコード集です


## joystic_example

ジョイスティックでロボットを操縦するコード例です。

次のコマンドでノードを起動します。

```sh
roslaunch consai2_examples joystic_example.launch
```

### 実機とgrSimの切り替え方法

`consai2_examples/launch/joystic_example.launch`を編集するか、
launchファイルの引数で設定できます。

```sh
# 例:実機を動かす場合
roslaunch consai2_examples joystic_example.launch sim:=false

# 例:grSimのロボットを動かす場合
roslaunch consai2_examples joystic_example.launch sim:=true
```

### 制御の切り替え方法


```sh
# 例:consai2_controlを経由する場合
roslaunch consai2_examples joystic_example.launch direct:=false

# 例:consai2_controlを経由せず、直接操縦する場合
roslaunch consai2_examples joystic_example.launch direct:=tru
```

### キー割り当ての変更

デフォルトのキー割り当てはこちらです。
ジョイスティックは[Logicool Wireless Gamepad F710](https://support.logicool.co.jp/ja_jp/product/wireless-gamepad-f710)
を使っています。

![Key_Config](https://github.com/SSL-Roots/consai2/blob/images/images/key_config_direct.png)

`consai2_examples/launch/joystic_example.launch`のキー番号を編集することで、キー割り当てを変更できます。

デフォルトのキー番号はこちらです。

![Key_Numbers](https://github.com/SSL-Roots/consai2/blob/images/images/key_numbders.png)
