import 'package:shared_preferences/shared_preferences.dart';
import '../models/user.dart';

class UserRepository {
  static const String _phoneNumberKey = 'user_phone_number';
  static const String _userNameKey = 'user_name';

  static UserRepository? _instance;
  final SharedPreferences _prefs;

  UserRepository._(this._prefs);

  static Future<UserRepository> getInstance() async {
    if (_instance == null) {
      final prefs = await SharedPreferences.getInstance();
      _instance = UserRepository._(prefs);
    }
    return _instance!;
  }

  Future<bool> hasPhoneNumber() async {
    final phoneNumber = _prefs.getString(_phoneNumberKey);
    return phoneNumber != null && phoneNumber.isNotEmpty;
  }

  Future<User?> getUser() async {
    final phoneNumber = _prefs.getString(_phoneNumberKey);
    if (phoneNumber == null || phoneNumber.isEmpty) {
      return null;
    }

    final name = _prefs.getString(_userNameKey);

    return User(
      id: phoneNumber,
      phoneNumber: phoneNumber,
      name: name,
    );
  }

  Future<void> savePhoneNumber(String phoneNumber) async {
    await _prefs.setString(_phoneNumberKey, phoneNumber);
  }

  Future<void> saveName(String name) async {
    await _prefs.setString(_userNameKey, name);
  }

  Future<void> saveUser(User user) async {
    await _prefs.setString(_phoneNumberKey, user.phoneNumber);
    if (user.name != null) {
      await _prefs.setString(_userNameKey, user.name!);
    }
  }

  Future<void> clearUser() async {
    await _prefs.remove(_phoneNumberKey);
    await _prefs.remove(_userNameKey);
  }

  String? getPhoneNumberSync() {
    return _prefs.getString(_phoneNumberKey);
  }
}
