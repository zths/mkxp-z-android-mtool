package app.mtool.mtoolmobile.mkxpz;

import android.app.Activity;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.TextView;

public class AboutActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.about_splash);

        // 获取并显示版本信息
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            String versionName = packageInfo.versionName;
            int versionCode = packageInfo.versionCode;

            TextView versionText = findViewById(R.id.version_text);
            versionText.setText(String.format(getString(R.string.version_info), versionName, versionCode));

            // 确保归属文本可见
            TextView attributionText = findViewById(R.id.attribution_text);
            attributionText.setText(getString(R.string.project_attribution));

            // 确保应用名称可见
            TextView appNameText = findViewById(R.id.app_name_text);
            appNameText.setText(getString(R.string.app_name));
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
    }
}
